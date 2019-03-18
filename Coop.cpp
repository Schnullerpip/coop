//clang sources
#include"clang/AST/DeclCXX.h"
#include"clang/Rewrite/Core/Rewriter.h"
//std libraries
#include<stdlib.h>
#include<math.h>
#include<algorithm>
#include<regex>
//linux libraries
#include<unistd.h>
#include<stdio.h>
//custom includes
#include"coop_utils.hpp"
#include"SystemStateInformation.hpp"
#include"MatchCallbacks.hpp"
#include"SourceModification.h"
#include"InputArgs.h"
#include"data.hpp"

using namespace clang::tooling;
using namespace llvm;

using namespace clang;
using namespace clang::ast_matchers;

#define coop_default_cold_data_allocation_size_i 1024
#define coop_default_hot_data_allocation_size_i 1024
#define coop_hot_split_tolerance_f .17f
#define coop_field_weight_depth_factor_f 10.f

#define coop_free_list_template_file_name "free_list_template.hpp"

// -------------- GENERAL STUFF ----------------------------------------------------------
// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");
// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\ncoop does stuff! neat!");
// -------------- GENERAL STUFF ----------------------------------------------------------

void recursive_weighting(
	coop::record::record_info *rec_ref,
	coop::fl_node *node
);

void recursive_weighting(
	coop::record::record_info *rec_info,
	const Stmt* child_loop,
	int depth = 1
);

void recursive_loop_memberusage_aggregation(
	const Stmt* parent,
	const Stmt* child);

void create_member_matrices(
	coop::record::record_info *record_stats
);
void fill_function_member_matrix(
	coop::record::record_info &rec_ref,
	std::set<const FieldDecl*> *fields
);
void fill_loop_member_matrix(
	coop::record::record_info &rec_ref,
	std::set<const FieldDecl*> *fields
);

float field_weight_depth_factor_g = coop_field_weight_depth_factor_f;

coop::system::cache_credentials l1;

//main start
int main(int argc, const char **argv) {
		//register the tool's options
		float hot_split_tolerance = .17f;

		{
			auto split_tolerance_action = [&hot_split_tolerance](std::vector<std::string> args){
				hot_split_tolerance = atof(args[0].c_str());
				coop::logger::log_stream << "[Config]::hot-split tolerance factor set to " << hot_split_tolerance;
				coop::logger::out();
			};
			coop::input::register_parametered_config("split-tolerance", split_tolerance_action);
		}

		bool apply_changes_to_source_files = true;
		coop::input::register_parameterless_config("analyze-only", [&apply_changes_to_source_files](){
			apply_changes_to_source_files = false;
			coop::logger::out("[Config]::coop will not apply any sorce transformations");
			});
		
		std::vector<std::string> exclude_folders;
		coop::input::register_parametered_config("exclude-folders", [&exclude_folders](std::vector<std::string> args){
			exclude_folders = args;
			coop::logger::out("[Config]::coop will exclude src files from the following directories :")++;
			for(auto ex : exclude_folders)
			{
				coop::logger::out(ex.c_str());
			}
			coop::logger::depth--;
		});

		std::set<std::string> include_files;
		coop::input::register_parametered_config("include-files", [&include_files](std::vector<std::string> args){
			include_files.insert(args.begin(), args.end());
			coop::logger::out("[Config]::following header files will explicitly be regarded:")++;
			for(auto header : include_files)
			{
				coop::logger::out(header.c_str());
				coop::match::add_file_as_match_condition(header.c_str());
			}
			coop::logger::depth--;
		});

		size_t number_hot_data_elements = coop_default_hot_data_allocation_size_i;
		coop::input::register_parametered_config("hot-elements", [&number_hot_data_elements](std::vector<std::string> args){
			number_hot_data_elements = atoi(args[0].c_str());
			coop::logger::log_stream << "[Config]::default hot data elements number is set to: " << number_hot_data_elements;
			coop::logger::out();
		});

		size_t number_cold_data_elements = coop_default_cold_data_allocation_size_i;
		coop::input::register_parametered_config("cold-elements", [&number_cold_data_elements](std::vector<std::string> args){
			number_cold_data_elements = atoi(args[0].c_str());
			coop::logger::log_stream << "[Config]::default cold data elements number is set to: " << number_cold_data_elements;
			coop::logger::out();
		});

		coop::input::register_parametered_config("depth-factor", [](std::vector<std::string> args){
			field_weight_depth_factor_g = atof(args[0].c_str());
			coop::logger::log_stream << "[Config]::default field weifht depth factor is set to: " << field_weight_depth_factor_g;
			coop::logger::out();
		});

		std::string user_include_path_root = "";
		coop::input::register_parametered_config("user-include-root", [&user_include_path_root](std::vector<std::string> args){
			user_include_path_root = args[0];
			coop::logger::log_stream << "[Config]::users include root: " << user_include_path_root << " (new files might be placed here)";
			coop::logger::out();
		});

		bool user_wants_to_confirm_each_record = false;
		coop::input::register_parameterless_config("confirm-record-changes", [&user_wants_to_confirm_each_record](){
			user_wants_to_confirm_each_record = true;
			coop::logger::out("[Config]::coop will ask for permission before each source transformation");
		});

	coop::logger::log_stream << Format::bold_on << "SYSTEM SETUP" << Format::bold_off;
	coop::logger::out(coop::logger::RUNNING)++;

		coop::input::resolve_config();

		coop::logger::out("retreiving system information", coop::logger::RUNNING)++;

			l1 = coop::system::get_d_cache_info(coop::system::IDX_0);
			coop::logger::log_stream
				<< "cache lvl: " << l1.lvl
				<< " size: " << l1.size
				<< "KB; lineSize: " << l1.line_size << "B";

		coop::logger::out()--;
		coop::logger::out("retreiving system information", coop::logger::DONE);

		//registering all the user specified files
		std::vector<std::string> user_files;
		for(int i = 1; i < argc; ++i){
			if(strcmp(argv[i], "--") == 0)
				break;
			coop::logger::log_stream << "adding " << argv[i] << " to user source files";
			coop::logger::out();
			user_files.push_back(argv[i]);
			coop::match::add_file_as_match_condition(argv[i]);
		}

		//generate the clang tools
		char cwd_buff[FILENAME_MAX];
		if(!getcwd(cwd_buff, FILENAME_MAX)){
			coop::logger::log_stream << "could not get the current working directory path...";
			coop::logger::err(coop::YES);
		}
		std::string cwd_string(cwd_buff), error_string("");
		auto compilation_database = clang::tooling::CompilationDatabase::autoDetectFromDirectory(cwd_string, error_string);
		if(!error_string.empty())
		{
			coop::logger::log_stream << error_string;
			coop::logger::err(coop::YES);
		}

		//fill the user files with all files from the compilation database - if there is none, none is added -> this allows for duplicates!!!
		if(user_files.empty()){//if the user said which file/s to analyze, don't retrieve this information from a compilation database
			coop::logger::out("retreiving source files, that will be analyzed", coop::logger::RUNNING)++;
			for(auto file : compilation_database->getAllFiles())
			{
				bool is_supposed_to_be_excluded = false;
				for(auto ex : exclude_folders)
				{
					//if this file is located in one of the folders, that are supposed to be excluded - ignore it
					std::stringstream path_to_find;
					path_to_find << cwd_buff << "/" << ex;
					size_t found_position = file.find(ex);
					if(found_position != std::string::npos)
					{
						is_supposed_to_be_excluded = true;
						break;
					}
				}
				if(is_supposed_to_be_excluded)
					continue;
				user_files.push_back(file);
				coop::match::add_file_as_match_condition(file.c_str());
			}

			//print the files
			for(auto file : user_files)
			{
				coop::logger::out(file.c_str());
			}
			coop::logger::depth--;
			coop::logger::out("retreiving source files, that will be analyzed", coop::logger::DONE);
		}

		//CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
		ClangTool Tool(*compilation_database, user_files);
		//those don't work...
		//Tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster("-march=x86-64"));
		Tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster("-m64"));

		//with the user files defined as a match regex we can now initialize the matchers
		std::string file_match_string = coop::match::get_file_regex_match_condition(((!include_files.empty() || user_include_path_root.empty()) ? nullptr : user_include_path_root.c_str()));

		//create a matcher that filters only the user files - so we dont end up making changes in system headers
		auto file_match = isExpansionInFileMatching(file_match_string);

        DeclarationMatcher classes = cxxRecordDecl(file_match, hasDefinition(), unless(anyOf(isUnion(), isImplicit()))).bind(coop_class_s);
		DeclarationMatcher members = fieldDecl(file_match, hasAncestor(cxxRecordDecl(hasDefinition(), anyOf(isClass(), isStruct())).bind(coop_class_s))).bind(coop_member_s);
		DeclarationMatcher function_prototypes = functionDecl(file_match, unless(isDefinition())).bind(coop_function_s);
		StatementMatcher members_used_in_functions = memberExpr(file_match, hasAncestor(functionDecl(isDefinition()).bind(coop_function_s))).bind(coop_member_s);
        StatementMatcher loops = 			anyOf(forStmt(file_match).bind(coop_loop_s), whileStmt(file_match).bind(coop_loop_s));
        StatementMatcher loops_distinct = 	anyOf(forStmt(file_match).bind(coop_for_loop_s), whileStmt(file_match).bind(coop_while_loop_s));
        StatementMatcher loops_distinct_each = eachOf(forStmt(file_match).bind(coop_for_loop_s), whileStmt(file_match).bind(coop_while_loop_s));
        StatementMatcher function_calls_in_loops = callExpr(file_match, hasAncestor(loops)).bind(coop_function_call_s);
        auto has_loop_ancestor = hasAncestor(loops_distinct_each);

		StatementMatcher ff_child_parent = callExpr(callee(functionDecl(file_match).bind(coop_child_s)), hasAncestor(functionDecl().bind(coop_parent_s)));
		StatementMatcher fl_child_parent = callExpr(callee(functionDecl(file_match).bind(coop_child_s)), hasAncestor(loops_distinct));

		StatementMatcher ll_child_parent = 	anyOf(forStmt(file_match, has_loop_ancestor).bind(coop_child_for_loop_s),
												  whileStmt(file_match, has_loop_ancestor).bind(coop_child_while_loop_s));
		StatementMatcher lf_child_parent = 	stmt(anyOf(forStmt(file_match).bind(coop_child_for_loop_s), whileStmt(file_match).bind(coop_child_while_loop_s)), hasAncestor(functionDecl().bind(coop_parent_s)));

        StatementMatcher nested_loops =
            eachOf(forStmt(file_match, has_loop_ancestor).bind(coop_child_for_loop_s),
                  whileStmt(file_match, has_loop_ancestor).bind(coop_child_while_loop_s));

        StatementMatcher members_used_in_for_loops = memberExpr(hasAncestor(forStmt(file_match).bind(coop_loop_s))).bind(coop_member_s);
        StatementMatcher members_used_in_while_loops = memberExpr(hasAncestor(whileStmt(file_match).bind(coop_loop_s))).bind(coop_member_s);

		//StatementMatcher deleted_instance =
		//	anyOf(ignoringParenImpCasts(arraySubscriptExpr().bind(coop_array_idx_s)), ignoringParenImpCasts(declRefExpr().bind(coop_class_s)));

       // StatementMatcher delete_calls =
       //     cxxDeleteExpr(file_match, ignoringParenImpCasts(hasDescendant(deleted_instance))).bind(coop_deletion_s);

	   StatementMatcher delete_calls = cxxDeleteExpr(file_match).bind(coop_deletion_s);




		coop::MemberRegistrationCallback			member_registration_callback;
		coop::FunctionPrototypeRegistrationCallback prototype_registration_callback;
		coop::FunctionRegistrationCallback			member_usage_callback;
		coop::LoopMemberUsageCallback				for_loop_member_usages_callback;
		coop::LoopMemberUsageCallback				while_loop_member_usages_callback;
		coop::ParentedFunctionCallback				parented_function_callback;
		coop::ParentedLoopCallback					parented_loop_callback;

		MatchFinder data_aggregation;
		data_aggregation.addMatcher(classes, &member_registration_callback);
		data_aggregation.addMatcher(function_prototypes, &prototype_registration_callback);
		data_aggregation.addMatcher(members_used_in_functions, &member_usage_callback);

		data_aggregation.addMatcher(ff_child_parent, &parented_function_callback);
		data_aggregation.addMatcher(fl_child_parent, &parented_function_callback);
		data_aggregation.addMatcher(ll_child_parent, &parented_loop_callback);
		data_aggregation.addMatcher(lf_child_parent, &parented_loop_callback);

		data_aggregation.addMatcher(members_used_in_for_loops, &for_loop_member_usages_callback);
		data_aggregation.addMatcher(members_used_in_while_loops, &while_loop_member_usages_callback);

	coop::logger::depth--;
	coop::logger::log_stream << Format::bold_on << "SYSTEM SETUP" << Format::bold_off;
	coop::logger::out(coop::logger::DONE);

	coop::logger::log_stream << Format::bold_on << "Data Aggregation" << Format::bold_off << " (parsing AST and invoking callback routines)";
	coop::logger::out(coop::logger::RUNNING)++;

		//generate the ASTs for each compilation unit
		std::vector<std::unique_ptr<ASTUnit>> ASTs;
		int system_state = Tool.buildASTs(ASTs);

		//run the matchers/callbacks
		for(unsigned i = 0; i < ASTs.size(); ++i){
			coop::logger::log_stream << "matching against AST[" << i << "]";
			coop::logger::out()++;
			data_aggregation.matchAST(ASTs[i]->getASTContext());
			coop::logger::depth--;
		}

		//print out the found records (classes/structs) and their fields
		coop::logger::out("Found Records:")++;
		member_registration_callback.printData();

		coop::logger::depth--;
		coop::logger::depth--;
	coop::logger::log_stream << Format::bold_on << "Data Aggregation" << Format::bold_off << " (parsing AST and invoking callback routines)";
	coop::logger::out(coop::logger::DONE);





	//if there are no records or no records were found just stop..
	if(member_registration_callback.class_fields_map.empty())
	{
		coop::logger::out("No Records (Classes/Structs) were found in the given sources");
		if(user_include_path_root.empty())
		{
			coop::logger::log_stream << "There is no 'user-include-root' set in coop's local config file! This might very well be the issue!";
			coop::logger::out();
		}
		if(user_files.empty())
		{
			coop::logger::log_stream << "There are no user files - you need to either provide the source files as command line arguments, or have a compile_commands.json file at the root directory";
			coop::logger::out();
		}
		coop::logger::out("System exit");
		exit(1);
	}




	coop::logger::log_stream << Format::bold_on << "determining logically related fields" << Format::bold_off;
	coop::logger::out(coop::logger::RUNNING)++;
		const int num_records = member_registration_callback.class_fields_map.size();

		//creating record_info for each record
		coop::record::record_info *record_stats =
			new coop::record::record_info[num_records]();

		coop::logger::out("creating the member matrices", coop::logger::RUNNING)++;

		/*
		now we have all functions, using members -> FunctionRegistrationCallback::relevant_functions
		and we also have all loops, using members-> LoopMemberUsageCallback::loops 

		to be nest complete it is not enough to now search for all functions containing the relevant loops and vice versa because this can be nested infinitely/theoretically... 
		we need to recursively search for higher instances of calls -> e.g. loop A is relevant -> search for EACH function as well as EACH loop associating it -> we find function B
		Since B is now relevant search for EACH function/loop associating it etc. Recursion needs to be considered and ideally somehow remembered as a priority weighing factor since recursion will act loop-like
		*/

		//coop::logger::out("ptr_IDs - functions")++;
		//for(auto &pi : coop::global<FunctionDecl>::ptr_id)
		//{
		//	coop::logger::log_stream << pi.ptr << " " << pi.id ;
		//	coop::logger::out();
		//}
		//coop::logger::depth--;
		//coop::logger::out("ptr_IDs - loops")++;
		//for(auto &pi : coop::global<Stmt>::ptr_id)
		//{
		//	coop::logger::log_stream << pi.ptr << " " << pi.id ;
		//	coop::logger::out();
		//}
		//coop::logger::depth--;
		//coop::logger::out("fl_nodes - f")++;
		//for(auto &fn : coop::AST_abbreviation::function_nodes)
		//{
		//	coop::logger::log_stream << fn.second->ID();
		//	coop::logger::out();
		//}
		//coop::logger::depth--;
		//coop::logger::out("fl_nodes - l")++;
		//for(auto &fl : coop::AST_abbreviation::loop_nodes)
		//{
		//	coop::logger::log_stream << fl.second->ID();
		//	coop::logger::out();
		//}
		//coop::logger::depth--;


		//traverse the AST_abbreviation to find out wether or not recursive function calls exist and remember them
		coop::AST_abbreviation::determineRecursion();

		//reduce the AST abbreviation to the relevant functions/loops
		//we know which functions/loops are relevant directly -> FunctionRegistrationCallback::relevant_functions LoopMemberUsageCallback::loops
		//now for each of those relevant entities, we need to iterate over their parents, marking them as relevant too and registering them, so they can be accounted for
		coop::AST_abbreviation::reduceASTabbreviation();

		//search for all the leaf nodes
		coop::AST_abbreviation::determineLeafNodes();

		/*there can be member usages in a loop either by direct usage or inlined functions
		BUT there can also be indirect member usage in a loop by calling a function, that uses a member
		so we also need to consider all the functions, that are being called in a loop, and check, wether or
		not they use relevant members*/
		coop::AST_abbreviation::attributeNestedMemberUsages();

		//for debugging - print the ast abbreviation from bottom up
		//for(auto n : coop::AST_abbreviation::leaf_nodes)
		//{
		//	coop::AST_abbreviation::print_parents(n);
		//}


		//determine the loop depth of the nodes
		//We should now have record_infos with valid information on which members are used by which function/loop
		//we also have the information on which loop/func parents which other loops/functions
		//our approach will be to determine the loopdepth of each variable to approximate its 'weight' or 'usage frequency'
		//and therefore it's importance to the performance of the program
		//we can't reason about which loop is most important (probably), but we can take an educated guess by saying:
		//"greater loopdepth means the chance of this field being used often is also greater"
		coop::AST_abbreviation::determineLoopDepths();

		/*now we know the classes (and their members) and the functions as well as all the loops, that use those members
		now for each class we need to pick their members inside the functions/loops, to see which ones are related*/
		create_member_matrices(record_stats);

		coop::logger::depth--;
		coop::logger::out("creating the member matrices", coop::logger::DONE)--;

	coop::logger::depth--;
	coop::logger::log_stream << Format::bold_on << "determining logically related fields" << Format::bold_off;
	coop::logger::out(coop::logger::DONE);

	coop::logger::log_stream << Format::bold_on << "Applying Heuristic" << Format::bold_off << " (prioritize pairings)";
	coop::logger::out(coop::logger::RUNNING)++;

		//now that we have a matrix for each record, that tells us which of its members are used in which function how many times,
		//we can take a heuristic and prioritize pairings
		//by determining which of the members are used most frequently together, we know which ones to make cachefriendly
		float *record_field_weight_average = new float[num_records]();

		for(int i = 0; i < num_records; ++i){
			coop::record::record_info &rec = record_stats[i];
			size_t record_size = coop::determine_size_with_padding(rec.record);

			coop::logger::log_stream << Format::bold_on << "Record " << Format::bold_off << Format::blue << rec.record->getNameAsString().c_str() << Format::def << "(" << record_size << "Byte)";
			coop::logger::out()++;

				float average = 0;

				//check the functions - if there is a recursive function, make sure to add its members' field weights to the average
				//for(auto func_mems : *rec.relevant_functions)
				//{
				//	const FunctionDecl *func = func_mems.first;
				//	//get node
				//	auto function_node = coop::AST_abbreviation::function_nodes.find(func)->second;
				//	if(!function_node->recursive_calls.empty())
				//	{
				//		//get fnc_idx
				//		int function_idx = coop::FunctionRegistrationCallback::function_idx_mapping[func];
				//		auto mems = func_mems.second;
				//		for(auto mem : mems)
				//		{
				//			int mem_idx = rec.isRelevantField(mem);
				//			if(mem_idx >= 0)
				//			{
				//				float weight = rec.fun_mem.at(mem_idx, function_idx);
				//				rec.field_weights[mem_idx].second += weight;
				//				average += weight;
				//			}
				//		}
				//	}
				//}

				//each record may have several fields - iterate them
				int num_fields = rec.field_idx_mapping.size();
				float max = 0;
				for(auto fi : rec.field_idx_mapping){
					int field_idx = fi.second;
					//add up the column of the weighted loop_mem map 
					float accumulated_field_weight = 0;
					for(unsigned y = 0; y < rec.relevant_loops->size(); ++y){
						accumulated_field_weight += rec.loop_mem.at(field_idx, y);
					}
					//add up the column of the weighted fun_mem map 
					for(unsigned y = 0; y < rec.relevant_functions->size(); ++y){
						accumulated_field_weight += rec.fun_mem.at(field_idx, y);
					}
					rec.field_weights[field_idx].second = accumulated_field_weight;
					if(max < accumulated_field_weight){
						max = accumulated_field_weight;
					}
					average += accumulated_field_weight;
				}
				

				record_field_weight_average[i] = average = average/num_fields;

				/*with the field weight averages (FWAs) we can now narrow down on which members are hot and cold (presumably)
				we also now know which fields are logically linked (loop/member matrix)
					-> which fields show temporal locality
						-> fields appearing in the same rows are logically linked
				hot members will stay inside the class definition
				cold members will be transferred to a struct, that defines those members as part of it and a
				reference to an instance of said struct will be placed in the original record's definition

				several cases need to be considered:
					-> one hot field not linked to any other fields -> 'special snowflake'
						-> should EVERYTHING else be externalized to the cold struct?
					-> several hot, logically linked field tuples -> this actually shows a lack of cohesion and could/should be communicated as a possible designflaw -> can/should I fix this?
					-> cold data that has temporal locality to hot data (used in same loop as hot data, but not nearly as often (only possible for nested Loops: loop A nests loop B; A uses cold 'a' B uses hot 'b' and 'c')) -> should a now be considered hot?
						'a' should probably just be handled locally (LHS - principle) but is this the purpose of this optimization?
					-> everything is hot/cold -> basically nothing to hot/cold split -> AOSOA should be applied
					-> 
				
				what about weightings in general? Should they only be regarded relatively to each other,
				or should I declare constant weight levels, that indicate wether or not data is hot/cold?
				*/

				//now determine the record's hot/cold fields

				//--------------------------significance ordering
				//sort the elements in a descending order
				std::sort(rec.field_weights.begin(), rec.field_weights.end(), [](
					std::pair<const clang::FieldDecl*, float>& e1,
					std::pair<const clang::FieldDecl*, float>& e2)
					{return e1.second > e2.second;});
				//get all the weights/sizes/alignment requirements of the fields
				std::vector<coop::weight_size> weights(rec.field_weights.size());
				for(unsigned int i = 0; i < rec.field_weights.size(); ++i)
				{
					auto &f_w = rec.field_weights[i];

					size_t field_size = coop::get_sizeof_in_byte(f_w.first);
					size_t alignment_requirement = field_size;
					auto type = f_w.first->getType().getTypePtr();
					if(type->isArrayType()){
						alignment_requirement = f_w.first->getASTContext().
                 			getTypeSizeInChars(type->getArrayElementTypeNoTypeQual()).
							getQuantity();
					}
					weights[i] = {f_w.second, field_size, alignment_requirement};
				}

				coop::logger::out("Significance groups:");
				coop::logger::depth++;
				coop::SGroup * significance_groups = coop::find_significance_groups(&weights[0], 0, weights.size());

				significance_groups->print(true);
				coop::logger::depth--;


				coop::SGroup *p = significance_groups;
				float sum_max_field_weight = 0;
				//determine each groups traits (typesize, highest fieldweight)
				do{
					//iterate over the fields, that this group encompasses
					for(unsigned int i = p->start_idx; i <= p->end_idx; ++i)
					{
						auto &f_w = rec.field_weights[i];
						p->type_size += coop::get_sizeof_in_byte(f_w.first);
						if(f_w.second > p->highest_field_weight)
						{
							p->highest_field_weight = f_w.second;
						}
					}
					p->finalize(weights);
					sum_max_field_weight += p->highest_field_weight;
				}while((p=p->next));

				//now apply the heuristic to the groups to find a possible split
				bool found_split = false;
				//size_t Si_prev = significance_groups->type_size;
				size_t CLS = l1.line_size;
				float sum_max_k = significance_groups->highest_field_weight;
				p=significance_groups->next; //splitting makes only sence from the 2nd group (we dont want to split everything...)
				if(p){
					coop::SGroup *prev = significance_groups;
				do{
					coop::logger::log_stream << "considering split: " << p->get_string();
					coop::logger::out()++;

					//Sizes S of assembled groups indices symbolize group range -> e.g. S0_i = size of groups from group 0 to group i (current p)
					size_t S0_i = coop::determine_size_with_optimal_padding(significance_groups, p, sizeof(void*));
					size_t Si_prev = coop::determine_size_with_optimal_padding(significance_groups, prev, sizeof(void*));
					size_t Si_n = coop::determine_size_with_optimal_padding(p, nullptr);

					coop::logger::out("Size:")++;
					coop::logger::log_stream << "hot data with " << p->get_string()<< ": " << S0_i;
					coop::logger::out();
					coop::logger::log_stream << "hot data without " << p->get_string()<< ": " << Si_prev;
					coop::logger::out();
					coop::logger::log_stream << "cold without " << p->get_string()<<": " << Si_n;
					coop::logger::out()--;

					//first requirement -> either reduce number cachelines or number elements per cache-line (or both of course - we love both)
					float cache_lines_per_element_with = S0_i*1.f/CLS;
					float cache_lines_per_element_without = Si_prev*1.f/CLS;
					float elements_per_cache_line_with = CLS*1.f/S0_i;
					float elements_per_cache_line_without = CLS*1.f/Si_prev;

					bool reduces_elements_per_cache_line = ((record_size < CLS) && (std::ceil(elements_per_cache_line_without) > std::ceil(elements_per_cache_line_with)));
					bool reduces_cache_lines_per_element = ((record_size > CLS) && (std::ceil(cache_lines_per_element_without) < std::ceil(cache_lines_per_element_with)));
					coop::logger::log_stream << "reduces elems per cache-line: " << (reduces_elements_per_cache_line ? "yes" : "no");
					coop::logger::out();
					coop::logger::log_stream << "reduces cache-lines per elem: " << (reduces_cache_lines_per_element ? "yes" : "no");
					coop::logger::out();

					if(reduces_cache_lines_per_element || reduces_elements_per_cache_line){
						//check whether the cost/benefit ratio is good
						//variable names refer to formula in thesis (s = savings for group i split; o = overhead for group i split)
						float si = sum_max_k * (-sizeof(void*) + Si_n)/1.f*CLS;
						float oi = (sum_max_field_weight - sum_max_k) * (1 + Si_n)/1.f*CLS;

						coop::logger::log_stream << "s(gi): " << si << ", o(gi): " << oi;
						coop::logger::out();

						bool Wi = si > oi;
						if(Wi){
							//this split is worht!
							found_split = true;
							coop::logger::depth--;
							break;
						}
					}

					sum_max_k += p->highest_field_weight;
					prev = p;
					coop::logger::depth--;
				} while((p=p->next));
				}
				//--------------------------significance ordering



				float tolerant_average = average * (1-hot_split_tolerance);
				float one_minus_avg = max - average;
				float top_2 = std::max<float>(average, one_minus_avg)/2;
				coop::logger::out("Heuristics:");
				coop::logger::log_stream << "W [In use]: " << (found_split ? p->highest_field_weight : -1);
				coop::logger::out();
				coop::logger::log_stream << "avg: " << average;
				coop::logger::out();
				coop::logger::log_stream << "1-avg: " << one_minus_avg;
				coop::logger::out();
				coop::logger::log_stream << "top/2: " << top_2;
				coop::logger::out();



				//float heuristic = tolerant_average;
				coop::logger::out("h/c\tname(size)\tfield weight:");
				float heuristic = (found_split ? p->highest_field_weight : -1);
				for(auto f_w : rec.field_weights){
					if(f_w.second <= heuristic){
						rec.cold_fields.push_back(f_w.first);
						coop::logger::log_stream << "[cold]\t" ;
					}else{
						rec.hot_fields.push_back(f_w.first);
						coop::logger::log_stream << "[hot]\t";
					}
					coop::logger::log_stream << f_w.first->getNameAsString() << "(" << 
					coop::get_sizeof_in_byte(f_w.first) << " B) \t" << f_w.second;
					coop::logger::out();
				}
			coop::logger::depth--;
		}


	coop::logger::depth--;
	coop::logger::log_stream << Format::bold_on << "Applying Heuristic" << Format::bold_off << " (prioritize pairings)";
	coop::logger::out(coop::logger::DONE);

	//collect all cold field declarations
	//if there aren't any - we're done
	std::vector<const FieldDecl*> cold_members;
	for(int i = 0; i < num_records; ++i){
		auto &recs_cold_fields = record_stats[i].cold_fields;
		cold_members.insert(cold_members.end(), recs_cold_fields.begin(), recs_cold_fields.end());
	}
	if(cold_members.empty()){
		coop::logger::out("No beneficial split opportunity was found");
	}

	if(apply_changes_to_source_files && (!cold_members.empty())){
		coop::logger::log_stream << Format::bold_on << "Applying changes to source files" << Format::bold_off;
		coop::logger::out(coop::logger::RUNNING)++;
			//now that we know the hot/cold fields we now should process the source-file changes 

			/*first we need another data aggregation
				- find all occurances of cold member usages
				- find the relevant record's destructors
				- find occurences of splitted classes heap instantiations
				- find occurences of splitted classes deletions
				- find splitted classes' constructors/copy constructors/copyAssignmentOperators
			*/

			MatchFinder finder;

			std::vector<coop::FindDestructor*> destructor_finders;
			coop::FindConstructor constructor_finder;
			coop::FindCopyAssignmentOperators copy_assignment_finder;
			coop::FindMoveAssignmentOperators move_assignment_finder;
			coop::FindInstantiations instantiation_finder;
			coop::FindDeleteCalls deletion_finder;
			coop::AccessSpecCallback access_spec_finder;

			for(int i = 0; i < num_records; ++i){
				auto &rec = record_stats[i];
				if(!rec.cold_fields.empty()){ //only consider splitted classes
					instantiation_finder.add_record(rec.record);
					deletion_finder.add_record(rec.record);
					constructor_finder.add_record(rec.record);
					copy_assignment_finder.add_record(rec.record);
					move_assignment_finder.add_record(rec.record);

					coop::FindDestructor *df = new coop::FindDestructor(rec);
					destructor_finders.push_back(df);
					std::stringstream ss;
					ss << "~" << rec.record->getNameAsString();
					finder.addMatcher(
						cxxDestructorDecl(file_match, isDefinition(), hasName(ss.str().c_str())).bind(coop_destructor_s),
						df);
				}
			}

			//to find the relevant instantiations
			finder.addMatcher(cxxRecordDecl(file_match, hasDescendant(accessSpecDecl().bind(coop_access_s))).bind(coop_class_s), &access_spec_finder);
			finder.addMatcher(delete_calls, &deletion_finder);
			finder.addMatcher(cxxNewExpr(file_match).bind(coop_new_instantiation_s), &instantiation_finder);
			finder.addMatcher(cxxConstructorDecl(file_match, isDefinition(), unless(isImplicit())).bind(coop_constructor_s), &constructor_finder);
			finder.addMatcher(cxxMethodDecl(file_match, isDefinition(), isCopyAssignmentOperator(), unless(isImplicit())).bind(coop_function_s), &copy_assignment_finder);
			finder.addMatcher(cxxMethodDecl(file_match, isDefinition(), isMoveAssignmentOperator(), unless(isImplicit())).bind(coop_function_s), &move_assignment_finder);

			//generate a regex matcher, that is able to find member usages (excluding those faulty registrations of records...)
			std::stringstream member_finder_regex;
			member_finder_regex << "(" << cold_members[0]->getNameAsString() << "|";
			if(cold_members.size() > 1){
				for(size_t i = 1;i < cold_members.size(); ++i)
				{
					member_finder_regex << cold_members[i]->getNameAsString();
					if(i < (cold_members.size()-1))
						member_finder_regex << "|";
				}
			}
			member_finder_regex << ")";

			auto name_matcher = matchesName(member_finder_regex.str());

			coop::logger::out("Second data aggregation - finding cold field usages relevant ctors/dtors/operators", coop::logger::RUNNING)++;
			//apply the matchers to all the ASTs
			for(unsigned i = 0; i < ASTs.size(); ++i){
				coop::logger::log_stream << "parsing AST[" << i << "]";
				coop::logger::out();
				MatchFinder find_cold_member_usages;
				ASTContext &ast_context = ASTs[i]->getASTContext();
				coop::ColdFieldCallback cold_field_callback(&cold_members, &ast_context);
				find_cold_member_usages.addMatcher(expr(ignoringImplicit(memberExpr(file_match, member(name_matcher)).bind(coop_member_s))), &cold_field_callback);

				find_cold_member_usages.matchAST(ast_context);
				finder.matchAST(ast_context);
			}
			coop::logger::depth--;
			coop::logger::out("Second data aggregation - finding cold field usages relevant ctors/dtors/operators", coop::logger::DONE);
			//destroy the destructor finders
			for(auto df : destructor_finders){
				delete df;
			}

			//traverse all the records -> if they have cold fields -> split them in a cold_data struct
			std::set<const char *> files_that_need_to_include_free_list;
			std::set<std::string> files_that_need_to_be_included_in_main;
			//will hold all additions that need to be made before the main function, so no duplicate location overwrites can occur
			std::stringstream injection_above_main;

			for(int i = 0; i < num_records; ++i){
				coop::record::record_info &rec = record_stats[i];


				//this check indicates wether or not the record has cold fields
				if(!rec.cold_fields.empty()){
					if(user_wants_to_confirm_each_record)
					{
						std::string input = "n";
						coop::logger::log_stream << "now touching " << rec.record->getNameAsString().c_str() << ". Allow coop to apply changes (y)?";
						coop::logger::out();
						if(!getline(std::cin, input))
						{
							coop::logger::out("cant process input");
						}
						if(!(input == "y" || input == "Y"))
						{
							continue;
						}
					}

					coop::logger::log_stream << "Applying source transformation for " << Format::blue << rec.record->getNameAsString().c_str() << Format::def;
					coop::logger::out(coop::logger::RUNNING);
					coop::logger::depth++;

					//local POD carrying context information from function to function
					coop::src_mod::cold_pod_representation cpr;


					cpr.rec_info = &rec;
					cpr.file_name = member_registration_callback.class_file_map[rec.record];
					cpr.record_name = rec.record->getNameAsString();
					cpr.user_include_path = user_include_path_root;
					cpr.qualified_record_name = rec.record->getQualifiedNameAsString();
					cpr.qualifier = coop::naming::get_without(cpr.qualified_record_name, cpr.record_name.c_str());

					//create a struct that holds all the field-declarations for the cold fields
					coop::src_mod::create_cold_struct_for(
						&rec,
						&cpr,
						user_include_path_root
					);
					
					//if the user did not give us an include path that we can copy the free_list template into 
					//then just inject it into the code directly
					if(user_include_path_root.empty()){
						coop::logger::out("injecting freelist code");
						coop::src_mod::create_free_list_for(&cpr);
					}

					////TODO this is not active....!
					//coop::logger::out("injecting memory allocations for the freelistinstances");
					//coop::src_mod::add_memory_allocation_to(
					//	&cpr,
					//	number_hot_data_elements,
					//	number_cold_data_elements
					//);

					coop::logger::out("Extracting cold fields");
					//get rid of all the cold field-declarations in the records
					// AND
					//change all appearances of the cold data fields to be referenced by the record's instance
					for(auto field : rec.cold_fields){
						coop::src_mod::remove_decl(field);
						auto field_usages = coop::ColdFieldCallback::cold_field_occurances[field];
						for(auto field_usage : field_usages){
							coop::src_mod::redirect_memExpr_to_cold_struct(
								field_usage.mem_expr_ptr,
								field,
								&cpr,
								field_usage.ast_context_ptr);
						}
					}
					coop::logger::out("Reordering hot data");
					coop::src_mod::reorder_hot_data(&cpr);

					//give the record a reference to an instance of this new cold_data_struct
					coop::logger::out("adding freelist pointr to class definition");
					coop::src_mod::add_cpr_ref_to(&cpr);

					//modifying/creating the record's important cops/move operators
					coop::logger::out("modifying/creating operators");
					coop::src_mod::handle_operators(&cpr);

					//modify/create the record's constructors
					coop::logger::out("modifying/creating constructors");
					coop::src_mod::handle_constructors(&cpr);

					coop::logger::out("injecting cold struct definitions");
					coop::src_mod::inject_cold_struct(&cpr);
					
					//modify/create the record's destructor, to handle free-list fragmentation
					coop::logger::out("modifying/creating destructor");
					coop::src_mod::handle_free_list_fragmentation(&cpr);

					//if this record is instantiated on the heap by using new anywhere, we
					//should make sure, that the single instances are NOT distributed in memory but rather be allocated with spatial locality
					coop::logger::out("changing 'new' usage");
					{
						auto iter = coop::FindInstantiations::instantiations_map.find(rec.record);
						if(iter != coop::FindInstantiations::instantiations_map.end()){
							//this record is apparently being instantiated on the heap!
							//make sure to replace those instantiations with something cachefriendly
							auto &instantiations = iter->second;
							for(auto expr_contxt : instantiations){
								coop::src_mod::handle_new_instantiation(&cpr, expr_contxt);
							}
						}
					}
					//accordingly there should be calls to the delete operator (hopefully)
					//if so we also need to modify those code bits, to make sure the free list wont fragment
					//so whenever an element is deleted -> tell the freelist to make space for new data
					coop::logger::out("changing 'delete' usage");
					{
						auto iter  = coop::FindDeleteCalls::delete_calls_map.find(rec.record);
						if(iter != coop::FindDeleteCalls::delete_calls_map.end()){
							auto &deletions = iter->second;
							for(auto del_contxt : deletions){
								coop::src_mod::handle_delete_calls(&cpr, del_contxt);
							}
						}
					}

					//since we made a split in this record - we need to define the freelist instances coming with it for each TU
					//if the record was defined in a cpp file - there is no need for that tho
					if(cpr.is_header_file){
						coop::logger::out("defining free list instances");
						coop::src_mod::define_free_list_instances(
							&cpr,
							injection_above_main,
							number_hot_data_elements,
							number_cold_data_elements,
							l1.line_size,
							l1.line_size,
							l1.line_size
						);
					}


					//throughout the transformations we might have found no entry point for an important change
					//to prevent duplicate location overwrites those changes are stored in cpr->missing_mandatory
					//now they can all be injected into the record at once
					coop::src_mod::handle_missing_mandatory(&cpr);

					//mark the record's translation unit as one, that needs to include the free_list_template implementation
					files_that_need_to_include_free_list.insert(member_registration_callback.class_file_map[rec.record].c_str());
					//if the splitted record was defined in a header the freelist instances will be made extern to be eventually defined
					//in the main file -> accordingly the main file needs to include the file that defined the record in the first place
					if(cpr.is_header_file){
						files_that_need_to_be_included_in_main.insert(cpr.file_name);
					}

					coop::logger::depth--;
					coop::logger::log_stream << "Applying source transformation for " << Format::blue << rec.record->getNameAsString() << Format::def;
					coop::logger::out(coop::logger::DONE);
				}
			}

			coop::src_mod::handle_injection_above_main(
				injection_above_main,
				coop::FunctionRegistrationCallback::main_function_ptr);

			//writes from the rewriter buffers into the actual files
			coop::src_mod::apply_changes();

			//if the user gave us an entry point to his/her include path -> drop the free_list_template.hpp in it
			//so it can be included by the relevant files
			if(!user_include_path_root.empty() && system_state != 1)
			{
				//add the freelist implementation to the user's include structure
				{
					std::stringstream ss;
					ss << "cp $" << COOP_TEMPLATES_PATH_NAME_S << "/" << coop_free_list_template_file_name << " "
						<< user_include_path_root << (user_include_path_root[strlen(user_include_path_root.c_str())-1] == '/' ? "" : "/") << coop_free_list_template_file_name;
					coop::logger::out(ss.str().c_str());
					if(system(ss.str().c_str()) != 0){
						coop::logger::out("[ERROR] can't touch user files for injecting #include code");
					}
				}
				for(auto f : files_that_need_to_include_free_list){
					coop::src_mod::include_free_list_hpp(f, coop_free_list_template_file_name);
				}
			}
			//make sure the main file knows about the splitted records
			if(coop::FunctionRegistrationCallback::main_function_ptr)
				for(auto f : files_that_need_to_be_included_in_main){
					coop::src_mod::include_file(coop::FunctionRegistrationCallback::main_file.c_str(), f.c_str());
				}
			else{
				coop::logger::log_stream << "couldn't find main file!";
				coop::logger::err(coop::Should_Exit::NO);
			}

		coop::logger::depth--;
		coop::logger::log_stream << Format::bold_on << "Applying changes to source files" << Format::bold_off;
		coop::logger::out(coop::logger::DONE);
	}

	coop::logger::log_stream << Format::bold_on << "System Cleanup" << Format::bold_off;
	coop::logger::out(coop::logger::RUNNING)++;
		for(auto &ptr_node : coop::AST_abbreviation::function_nodes)
		{
			delete ptr_node.second;
		}
		for(auto &ptr_node : coop::AST_abbreviation::loop_nodes)
		{
			delete ptr_node.second;
		}
		delete[] record_field_weight_average;
		delete[] record_stats;
	coop::logger::depth--;
	coop::logger::log_stream << Format::bold_on << "System Cleanup" << Format::bold_off;
	coop::logger::out(coop::logger::DONE);


	return 0;
}



void create_member_matrices( coop::record::record_info *record_stats)
{

	int rec_count = 0;
	for(auto class_fields_map : coop::MemberRegistrationCallback::class_fields_map){

		const CXXRecordDecl *rec = class_fields_map.first;
		const auto fields = &class_fields_map.second;

		//make sure to only work with the global instances
		auto global_rec = coop::global<CXXRecordDecl>::get_global(rec);
		if(!global_rec)
		{
			//there is no global record!?...
			coop::logger::out("found no global for a record, that cant be good... [Coop.cpp::create_member_matrices::~795]");
			continue;
		}
		rec = global_rec->ptr;

		coop::record::record_info &rec_ref = record_stats[rec_count];

		//initializing the info struct
		rec_ref.init(rec, fields,
			&coop::FunctionRegistrationCallback::relevant_functions,
			&coop::LoopMemberUsageCallback::loops);


		//iterate over each function to fill the function-member matrix of this record_info
		fill_function_member_matrix(
			rec_ref,
			fields);

		//iterate over each loop to fill the loop-member matrix of this record_info
		fill_loop_member_matrix(
			rec_ref,
			fields);

		coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [FUNCTION/member] matrix before weighting:"; coop::logger::out();
		rec_ref.print_func_mem_mat(coop::FunctionRegistrationCallback::function_idx_mapping);

		coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [LOOP/member] matrix before weighting:"; coop::logger::out();
		rec_ref.print_loop_mem_mat(coop::LoopMemberUsageCallback::loops, coop::LoopMemberUsageCallback::loop_idx_mapping);
		

		coop::logger::out("weighting nested nodes", coop::logger::RUNNING)++;
			//go through each loop and apply its depth to the rec_refs members
			for(auto &loop_credentials : *rec_ref.relevant_loops)
			{
				const Stmt *loop = loop_credentials.first;
				coop::loop_credentials &credentials = loop_credentials.second;

				//get the loops node (for the depth)
				auto node_iter = coop::AST_abbreviation::loop_nodes.find(loop);
				if(node_iter == coop::AST_abbreviation::loop_nodes.end()) {
					coop::logger::log_stream << "No node found for " << credentials.identifier;
					coop::logger::err(coop::Should_Exit::YES);
				}

				coop::fl_node *loop_node = node_iter->second;

				//get the loops idx in the rec_ref (so the right line in the loop_mem matrix is updated)
				auto loop_idx_iter = coop::LoopMemberUsageCallback::loop_idx_mapping.find(loop);
				if(loop_idx_iter == coop::LoopMemberUsageCallback::loop_idx_mapping.end())
				{
					coop::logger::log_stream << "No idx found for " << credentials.identifier;
					coop::logger::err(coop::Should_Exit::YES);
				}

				int loop_idx = loop_idx_iter->second;

				/*apply cross-referenced weighting - assuming temporal locality by node familiarity. Value this temporal locality
				we add the number associated members to the members' weights, so their temporal locality has a better chance to result
				in spatial locality by getting paired we do this BEFORE the vertical accumulation and BEFORE the depth weighting but AFTER
				the memberusage attribution, because the temporal locality should not be blurred by the field's overall weights and
				this way member relations through several nested loops are depicted accurately*/
				{
					int number_member_associations = 0;
					for(unsigned i = 0; i < rec_ref.fields.size(); ++i){
						if(rec_ref.loop_mem.at(i, loop_idx) > 0){
							number_member_associations += 1;
						}
					}
					if(number_member_associations > 0)
					{
						for(unsigned i = 0; i < rec_ref.fields.size(); ++i){
							float &field_weight = rec_ref.loop_mem.at(i, loop_idx);
							if(field_weight > 0)
							{
								field_weight += rec_ref.fields.size() - number_member_associations;
							}
						}
					}
				}

				//apply the loopdepth as the power of an arbitrary value to the weight (arbitrary because we can't really determine the actual 'weight' of a runtime dependent loop)
				for(unsigned i = 0; i < rec_ref.fields.size(); ++i){
					rec_ref.loop_mem.at(i, loop_idx) *= pow(field_weight_depth_factor_g, loop_node->getDepth());
				}
			}

			//go through each function and apply its depth to the rec_refs members if the functin is recursive, because in this case we treat it as a loop
			for(auto &func_mems : *rec_ref.relevant_functions)
			{
				const FunctionDecl *func_ptr = func_mems.first;

				//get the functions node (for the depth)
				auto node_iter = coop::AST_abbreviation::function_nodes.find(func_ptr);
				if(node_iter == coop::AST_abbreviation::function_nodes.end())
				{
					coop::logger::log_stream << "No node found for " << func_ptr->getNameAsString().c_str();
					coop::logger::err(coop::Should_Exit::YES);
				}
				coop::fl_node *function_node = node_iter->second;


				//get the loops idx in the rec_ref (so the right line in the loop_mem matrix is updated)
				auto func_idx_iter = coop::FunctionRegistrationCallback::function_idx_mapping.find(func_ptr);
				if(func_idx_iter == coop::FunctionRegistrationCallback::function_idx_mapping.end())
				{
					coop::logger::log_stream << "No idx found for " << function_node->ID();
					coop::logger::err(coop::Should_Exit::YES);
				}

				int func_idx = func_idx_iter->second;

				/*apply cross-referenced weighting - assuming temporal locality by node familiarity. Value this temporal locality
				we add the number associated members to the members' weights, so their temporal locality has a better chance to result
				in spatial locality by getting paired we do this BEFORE the vertical accumulation and BEFORE the depth weighting but AFTER
				the memberusage attribution, because the temporal locality should not be blurred by the field's overall weights and
				this way member relations through several nested loops are depicted accurately*/
				{
					int number_member_associations = 0;
					for(unsigned i = 0; i < rec_ref.fields.size(); ++i){
						if(rec_ref.fun_mem.at(i, func_idx) > 0){
							number_member_associations += 1;
						}
					}
					if(number_member_associations > 0)
					{
						for(unsigned i = 0; i < rec_ref.fields.size(); ++i){
							float &field_weight = rec_ref.fun_mem.at(i, func_idx);
							if(field_weight > 0)
							{
								field_weight += rec_ref.fields.size() - number_member_associations;
							}
						}
					}
				}

				//if the function is recursive, it actually has a depth, otherwise only loop nodes have a depth > 0
				if(function_node->getDepth() > 0 && !function_node->recursive_calls.empty())
				{
					//get that functions idx
					auto func_idx_iter = coop::FunctionRegistrationCallback::function_idx_mapping.find(func_ptr);
					if(func_idx_iter == coop::FunctionRegistrationCallback::function_idx_mapping.end())
					{
						coop::logger::log_stream << "No idx found for " << func_ptr->getNameAsString().c_str();
						coop::logger::err(coop::Should_Exit::YES);
					}
					int func_idx = func_idx_iter->second;

					for(unsigned i = 0; i < rec_ref.fields.size(); ++i){
						//since this IS  a nested loop (child loop) we can apply some arbitrary factor to the members'weights
						rec_ref.fun_mem.at(i, func_idx) *= pow(field_weight_depth_factor_g, function_node->getDepth());
					}
				}
			}
			
			coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [FUNCTION/member] matrix after weighting:"; coop::logger::out();
			rec_ref.print_func_mem_mat(coop::FunctionRegistrationCallback::function_idx_mapping);
			coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [LOOP/member] matrix after weighting:"; coop::logger::out();
			rec_ref.print_loop_mem_mat(coop::LoopMemberUsageCallback::loops, coop::LoopMemberUsageCallback::loop_idx_mapping);

		coop::logger::depth--;
		coop::logger::out("weighting nested loops", coop::logger::DONE);
		rec_count++;
	}
}

//iterate over each function to fill the function-member matrix of a record_info
void fill_function_member_matrix(
	coop::record::record_info &rec_ref,
	std::set<const FieldDecl*> *fields)
{
	//filter the unique memberexpr callee's from the relevant functions
	for(auto func_mems : coop::FunctionRegistrationCallback::relevant_functions){
		const FunctionDecl* func = func_mems.first;
		std::vector<const MemberExpr*> *mems = &func_mems.second;

		int func_idx = coop::FunctionRegistrationCallback::function_idx_mapping[func];

		std::map<std::string, std::set<const ValueDecl*>> uniques;

		//iterate over each memberexpression that function uses
		for(auto mem : *mems){

			//only work with the global instances
			const FieldDecl *field_ptr = static_cast<const FieldDecl*>(mem->getMemberDecl());
			auto global_field_ptr = coop::global<FieldDecl>::get_global(field_ptr);
			if(!global_field_ptr){
				continue;
			}
			field_ptr = global_field_ptr->ptr;

			//get the referenced decl if there is any and check if we already have it registered
			auto decRefExp = dyn_cast_or_null<DeclRefExpr>(*mem->child_begin());
			std::string decRefID = "this";

			{
				//if we can find a decRefExpr for this memExpr, retrieve its unique identifier - else it is the this pointer
				if(decRefExp)
				{
					decRefID = coop::naming::get_decl_id<NamedDecl>(decRefExp->getFoundDecl());
				}
				auto &value_decls = uniques[decRefID];
				if(value_decls.find(mem->getMemberDecl()) != value_decls.end()){
					//already registered this one - do nothing
					continue;
				}
				value_decls.insert(mem->getMemberDecl());
			}

			coop::logger::log_stream << "checking func '" << func->getNameAsString().c_str() << "'\thas member '"
				<< mem->getMemberDecl()->getNameAsString().c_str() << "' for record '" << rec_ref.record->getNameAsString().c_str();

			if(std::find(fields->begin(), fields->end(), field_ptr)!=fields->end() && field_ptr->getParent() == rec_ref.record){
				coop::logger::log_stream << "' - yes";
				rec_ref.fun_mem.at(rec_ref.field_idx_mapping[field_ptr], func_idx)++;
			}else{
				coop::logger::log_stream << "' - no";
			}
			coop::logger::out();
		}
	}
}

void fill_loop_member_matrix(
	coop::record::record_info &rec_ref,
	std::set<const FieldDecl*> *fields)
{
	auto &loop_mems_map = coop::LoopMemberUsageCallback::loops;
	auto &loop_idxs = coop::LoopMemberUsageCallback::loop_idx_mapping;

	for(auto loop_mems : loop_mems_map){
		auto loop = loop_mems.first;
		auto loop_info = &coop::LoopMemberUsageCallback::loops[loop];
		int loop_idx = loop_idxs[loop];

		std::map<std::string, std::set<const ValueDecl*>> uniques;

		//iterate over each member that loop uses
		for(auto mem : loop_info->member_usages){
			//only work with global instances -> also if there is no global this is probably a methodcall not a field usage
			auto global_field = coop::global<FieldDecl>::get_global(coop::naming::get_decl_id<ValueDecl>(mem->getMemberDecl()));
			if(!global_field)
			{
				continue;
			}
			const FieldDecl* field_ptr = global_field->ptr;

			clang::DeclRefExpr const *decRefExp = dyn_cast_or_null<DeclRefExpr>(*mem->child_begin());
			std::string decRefID = "this";


			{
				//if we can find a decRefExpr for this memExpr, retrieve its unique identifier - else it is the this pointer
				if(decRefExp) {
					decRefID = coop::naming::get_decl_id<NamedDecl>(decRefExp->getFoundDecl());
				}
				auto &value_decls = uniques[decRefID];
				if(value_decls.find(mem->getMemberDecl()) != value_decls.end()){
					//already registered this member usage for this instance - do nothing
					continue;
				}
				value_decls.insert(mem->getMemberDecl());
			}

			coop::logger::log_stream << "checking loop " << loop_info->identifier << " has member '"
				<< mem->getMemberDecl()->getNameAsString().c_str() << "' for record '" << rec_ref.record->getNameAsString().c_str();

			if(std::find(fields->begin(), fields->end(), field_ptr)!=fields->end() && field_ptr->getParent() == rec_ref.record){
				coop::logger::log_stream << "' - yes";
				rec_ref.loop_mem.at(rec_ref.field_idx_mapping[field_ptr], loop_idx)++;
			}else{
				coop::logger::log_stream << "' - no";
			}
			coop::logger::out();
		}
	}
}
