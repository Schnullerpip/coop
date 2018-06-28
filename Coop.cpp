//clang sources
#include "clang/AST/DeclCXX.h"
#include "clang/Rewrite/Core/Rewriter.h"
//std libraries
#include <stdlib.h>
//custom includes
#include "coop_utils.hpp"
#include "SystemStateInformation.hpp"
#include "MatchCallbacks.hpp"
#include "SourceModification.h"
#include "InputArgs.h"

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


//function heads
void recursive_weighting(
	coop::record::record_info *rec_info,
	const Stmt* child_loop);

void recursive_loop_memberusage_aggregation(
	const Stmt* parent,
	const Stmt* child);

void register_indirect_memberusage_in_loop_functioncalls(
	coop::LoopFunctionsCallback &loop_functions_callback,
	coop::FunctionRegistrationCallback &member_usage_callback
);
void create_member_matrices(
	coop::record::record_info *record_stats,
	coop::MemberRegistrationCallback &member_registration_callback,
	coop::FunctionRegistrationCallback &member_usage_callback,
	coop::LoopFunctionsCallback &loop_functions_callback,
	coop::NestedLoopCallback &nested_loop_callback
);
void fill_function_member_matrix(coop::record::record_info &rec_ref,
		std::vector<const FieldDecl*> *fields,
		coop::FunctionRegistrationCallback &member_usage_callback
);
void fill_loop_member_matrix(
	std::vector<const FieldDecl*> *fields,
	coop::record::record_info &rec_ref,
	coop::LoopFunctionsCallback &loop_functions_callback
);

float field_weight_depth_factor = coop_field_weight_depth_factor_f;



//main start
int main(int argc, const char **argv) {
		//register the tool's options
		float hot_split_tolerance = .17f;
		{
			auto split_tolerance_action = [&hot_split_tolerance](const char *size){hot_split_tolerance = atof(size);};
			coop::input::register_parametered_action("--split-tolerance", "split tolerance factor --split-tolreance <float>", split_tolerance_action);
			coop::input::register_parametered_action("-st", "split tolerance factor -st <float>", split_tolerance_action);
		}

		bool apply_changes_to_source_files = true;
		coop::input::register_parameterless_action("--analyze-only", "will not let coop do any actual changes to the source files", [&apply_changes_to_source_files](){apply_changes_to_source_files = false;});

		size_t hot_data_allocation_size_in_byte = coop_default_hot_data_allocation_size_i;
		coop::input::register_parametered_action("--hot-size", "set the default allocation size for hot data --hot-size <int>", [&hot_data_allocation_size_in_byte](const char *size){
			hot_data_allocation_size_in_byte = atoi(size);
		});

		size_t cold_data_allocation_size_in_byte = coop_default_cold_data_allocation_size_i;
		coop::input::register_parametered_action("--cold-size", "set the default allocation size for cold data --cold-size <int>", [&cold_data_allocation_size_in_byte](const char *size){
			cold_data_allocation_size_in_byte = atoi(size);
		});

		coop::input::register_parametered_action("--depth-factor", "decides depth dependent weighting of members --depth-factor 2.0", [](const char *factor){
			field_weight_depth_factor = atof(factor);
		});

		const char * user_include_path_root = nullptr;
		coop::input::register_parametered_action("-i", "coop will place files in this include root -i <directory_path>", [&user_include_path_root](const char * path){
			user_include_path_root = path;
			coop::logger::log_stream << "user's given include path is: " << user_include_path_root;
			coop::logger::out();
		});

		int clang_relevant_options =
			coop::input::resolve_actions(argc, argv);

	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::RUNNING)++;
		coop::logger::out("retreiving system information", coop::logger::RUNNING)++;

			coop::system::cache_credentials l1;
			l1 = coop::system::get_d_cache_info(coop::system::IDX_0);
			coop::logger::log_stream
				<< "cache lvl: " << l1.lvl
				<< " size: " << l1.size
				<< "KB; lineSize: " << l1.line_size << "B";
			coop::logger::out()--;
		coop::logger::out("retreiving system information", coop::logger::DONE);

		//registering all the user specified files
		std::vector<const char*> user_files;
		for(int i = 1; i < argc; ++i){
			if(strcmp(argv[i], "--") == 0)
				break;
			coop::logger::log_stream << "adding " << argv[i] << " to user source files";
			coop::logger::out();
			user_files.push_back(argv[i]);
			coop::match::add_file_as_match_condition(argv[i]);
		}

		//with the user files defined as a match regex we can now initialize the matchers
		auto file_match = isExpansionInFileMatching(coop::match::get_file_regex_match_condition(user_include_path_root));

        DeclarationMatcher classes = cxxRecordDecl(file_match, hasDefinition(), unless(isUnion())).bind(coop_class_s);
		DeclarationMatcher members = fieldDecl(file_match, hasAncestor(cxxRecordDecl(hasDefinition(), anyOf(isClass(), isStruct())).bind(coop_class_s))).bind(coop_member_s);
		StatementMatcher members_used_in_functions = memberExpr(file_match, hasAncestor(functionDecl().bind(coop_function_s))).bind(coop_member_s);

        StatementMatcher loops = anyOf(forStmt(file_match).bind(coop_loop_s), whileStmt(file_match).bind(coop_loop_s));
        StatementMatcher loops_distinct = anyOf(forStmt(file_match).bind(coop_for_loop_s), whileStmt(file_match).bind(coop_while_loop_s));
        StatementMatcher loops_distinct_each = eachOf(forStmt(file_match).bind(coop_for_loop_s), whileStmt(file_match).bind(coop_while_loop_s));
        StatementMatcher function_calls_in_loops = callExpr(file_match, hasAncestor(loops)).bind(coop_function_call_s);

        auto has_loop_ancestor = hasAncestor(loops_distinct_each);
        StatementMatcher nested_loops =
            eachOf(forStmt(file_match, has_loop_ancestor).bind(coop_child_for_loop_s),
                  whileStmt(file_match, has_loop_ancestor).bind(coop_child_while_loop_s));

        StatementMatcher members_used_in_for_loops =
            memberExpr(hasAncestor(forStmt(file_match).bind(coop_loop_s))).bind(coop_member_s);
        StatementMatcher members_used_in_while_loops =
            memberExpr(hasAncestor(whileStmt(file_match).bind(coop_loop_s))).bind(coop_member_s);

        StatementMatcher delete_calls =
            cxxDeleteExpr(file_match, hasDescendant(declRefExpr().bind(coop_class_s))).bind(coop_deletion_s);



		Rewriter rewriter;
		CommonOptionsParser OptionsParser(clang_relevant_options, argv, MyToolCategory);
		ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

		coop::MemberRegistrationCallback member_registration_callback(&user_files);
		coop::FunctionRegistrationCallback member_usage_callback(&user_files);
		coop::LoopFunctionsCallback loop_functions_callback(&user_files);
		coop::LoopMemberUsageCallback for_loop_member_usages_callback(&user_files);
		coop::LoopMemberUsageCallback while_loop_member_usages_callback(&user_files);
		coop::NestedLoopCallback nested_loop_callback(&user_files);

		MatchFinder data_aggregation;
		data_aggregation.addMatcher(classes, &member_registration_callback);
		data_aggregation.addMatcher(members_used_in_functions, &member_usage_callback);
		data_aggregation.addMatcher(function_calls_in_loops, &loop_functions_callback);
		data_aggregation.addMatcher(members_used_in_for_loops, &for_loop_member_usages_callback);
		data_aggregation.addMatcher(members_used_in_while_loops, &while_loop_member_usages_callback);
		data_aggregation.addMatcher(nested_loops, &nested_loop_callback);

		//generate the ASTs for each compilation unit
		std::vector<std::unique_ptr<ASTUnit>> ASTs;
		int system_state = Tool.buildASTs(ASTs);
	coop::logger::depth--;
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::DONE);

	coop::logger::out("data aggregation (parsing AST and invoking callback routines)", coop::logger::RUNNING)++;

		//run the matchers/callbacks
		for(unsigned i = 0; i < ASTs.size(); ++i){
			coop::logger::log_stream << "matching against AST[" << i << "]";
			coop::logger::out()++;
			data_aggregation.matchAST(ASTs[i]->getASTContext());
			coop::logger::depth--;
		}

		//auto front_end_action = newFrontendActionFactory(&data_aggregation).get();
		//Tool.run(front_end_action);

		//print out the found records (classes/structs) and their fields
		member_registration_callback.printData();
		coop::logger::depth--;
	coop::logger::out("data aggregation (parsing AST and invoking callback routines)", coop::logger::DONE);

	coop::logger::out("determining which members are logically related", coop::logger::RUNNING)++;
		const int num_records = member_registration_callback.class_fields_map.size();

		//creating record_info for each record
		coop::record::record_info *record_stats =
			new coop::record::record_info[num_records]();

		coop::logger::out("creating the member matrices", coop::logger::RUNNING)++;

		//the loop_registration_callback contains all the loops that directly associate members
		//loop_functions_callback contains all the loops, that call functions and therefore might indirectly associate members
		//find out which loop_functions_callback's functions are missing and extend the loop_registration
		register_indirect_memberusage_in_loop_functioncalls(
			loop_functions_callback,
			member_usage_callback);


		/*now we know the classes (and their members) and the functions as well as all the loops, that use those members
		now for each class we need to pick their members inside the functions, to see which ones are related*/
		create_member_matrices(
			record_stats,
			member_registration_callback,
			member_usage_callback,
			loop_functions_callback,
			nested_loop_callback);

		coop::logger::depth--;
		coop::logger::out("creating the member matrices", coop::logger::DONE)--;

	coop::logger::out("determining which members are logically related", coop::logger::DONE);

	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::RUNNING)++;

		//now that we have a matrix for each record, that tells us which of its members are used in which function how many times,
		//we can take a heuristic and prioritize pairings
		//by determining which of the members are used most frequently together, we know which ones to make cachefriendly
		float *record_field_weight_average = new float[num_records]();

		for(int i = 0; i < num_records; ++i){
			coop::record::record_info &rec = record_stats[i];
			float average = 0;

			//each record may have several fields - iterate them
			int num_fields = rec.field_idx_mapping.size();
			for(auto fi : rec.field_idx_mapping){
				int field_idx = fi.second;
				//add up the column of the weighted loop_mem map 
				float accumulated_field_weight = 0;
				for(unsigned y = 0; y < rec.relevant_loops->size(); ++y){
					accumulated_field_weight += rec.loop_mem.at(field_idx, y);
				}
				rec.field_weights[field_idx].second = accumulated_field_weight;
				average += accumulated_field_weight;

				coop::logger::log_stream << fi.first->getNameAsString().c_str() << "'s field weight is: " << rec.field_weights[field_idx].second;
				coop::logger::out();
			}

			record_field_weight_average[i] = average = average/num_fields;

			coop::logger::log_stream
				<< "record " << rec.record->getNameAsString().c_str()
				<< "'s field weight average = " << average;
			coop::logger::out();
        
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
				-> cold data that has temporal linkage to hot data (used in same loop as hot data, but not nearly as often (only possible for nested Loops: loop A nests loop B; A uses cold 'a' B uses hot 'b' and 'c')) -> should a now be considered hot?
					'a' should probably just be handled locally (LHS - principle) but is this the purpose of this optimization?
				-> everything is hot/cold -> basically nothing to hot/cold split -> AOSOA should be applied
				-> 
			
			what about weightings in general? Should they only be regarded relatively to each other,
			or should I declare constant weight levels, that indicate wether or not data is hot/cold?
			*/

			//now determine the record's hot/cold fields
			//the record's field_weights is now ordered (descending) so the moment we find a cold value
			//the following  values will also be cold
			coop::logger::depth++;
			float tolerant_average = average * (1-hot_split_tolerance);
			coop::logger::log_stream << rec.record->getNameAsString().c_str() << "'s tolerant average is: " << tolerant_average;
			coop::logger::out();
			for(auto f_w : rec.field_weights){
				if(f_w.second < tolerant_average){
					rec.cold_fields.push_back(f_w.first);
					coop::logger::log_stream << "[cold] " ;
				}else{
					coop::logger::log_stream << "[hot] ";
				}
				coop::logger::log_stream << f_w.first->getNameAsString();
				coop::logger::out();
			}
			coop::logger::depth--;
		}


	coop::logger::depth--;
	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::DONE);

	if(apply_changes_to_source_files){
		coop::logger::out("applying changes to source files", coop::logger::RUNNING);
			//now that we know the hot/cold fields we now should process the source-file changes 

			/*first we need another data aggregation
				- find all occurances of cold member usages
				- find the relevant record's destructors
			*/
			std::vector<const FieldDecl*> cold_members;
			for(int i = 0; i < num_records; ++i){
				auto &recs_cold_fields = record_stats[i].cold_fields;
				cold_members.insert(cold_members.end(), recs_cold_fields.begin(), recs_cold_fields.end());
			}

			MatchFinder finder;

			std::vector<coop::FindDestructor*> destructor_finders;
			coop::FindMainFunction find_main_function_callback(&user_files);
			coop::FindInstantiations instantiation_finder;
			coop::FindDeleteCalls deletion_finder;

			finder.addMatcher(delete_calls, &deletion_finder);
			finder.addMatcher(functionDecl(hasName("main")).bind(coop_function_s), &find_main_function_callback);
			for(int i = 0; i < num_records; ++i){
				auto &rec = record_stats[i];
				if(!rec.cold_fields.empty()){ //only consider splitted classes
					instantiation_finder.add_record(rec.record);
					deletion_finder.add_record(rec.record);

					coop::FindDestructor *df = new coop::FindDestructor(rec);
					destructor_finders.push_back(df);
					std::stringstream ss;
					ss << "~" << rec.record->getNameAsString();
					finder.addMatcher(
						cxxDestructorDecl(hasName(ss.str().c_str())).bind(coop_destructor_s),
						df);
				}
			}

			//to find the relevant instantiations
			finder.addMatcher(cxxNewExpr().bind(coop_new_instantiation_s), &instantiation_finder);

			//apply the matchers to all the ASTs
			for(unsigned i = 0; i < ASTs.size(); ++i){
				MatchFinder find_cold_member_usages;
				ASTContext &ast_context = ASTs[i]->getASTContext();
				coop::ColdFieldCallback cold_field_callback(&user_files, &cold_members, &ast_context);
				find_cold_member_usages.addMatcher(memberExpr().bind(coop_member_s), &cold_field_callback);

				find_cold_member_usages.matchAST(ast_context);
				finder.matchAST(ast_context);
			}
			//destroy the destructor finders
			for(auto df : destructor_finders){
				delete df;
			}

			//traverse all the records -> if they have cold fields -> split them in a cold_data struct
			std::set<const char *> files_that_need_to_include_free_list;
			for(int i = 0; i < num_records; ++i){
				coop::record::record_info &rec = record_stats[i];

				//this check indicates wether or not the record has cold fields
				if(!rec.cold_fields.empty()){
					coop::src_mod::cold_pod_representation cpr;

					//create a struct that holds all the field-declarations for the cold fields
					coop::src_mod::create_cold_struct_for(
						&rec,
						&cpr,
						user_include_path_root,
						hot_data_allocation_size_in_byte,
						cold_data_allocation_size_in_byte,
						&rewriter);
					
					//if the user did not give us an include path that we can copy the free_list template into 
					//then just inject it into the code directly
					if(!user_include_path_root){
						coop::src_mod::create_free_list_for(
							&cpr,
							&rewriter
						);
					}

					coop::src_mod::add_memory_allocation_to(
						&cpr,
						user_include_path_root,
						hot_data_allocation_size_in_byte,
						cold_data_allocation_size_in_byte,
						&rewriter
					);

					//get rid of all the cold field-declarations in the records
					// AND
					//change all appearances of the cold data fields to be referenced by the record's instance
					for(auto field : rec.cold_fields){
						coop::src_mod::remove_decl(field, &rewriter);
						auto field_usages = coop::ColdFieldCallback::cold_field_occurances[field];
						for(auto field_usage : field_usages){
							coop::src_mod::redirect_memExpr_to_cold_struct(field_usage.mem_expr_ptr, &cpr, field_usage.ast_context_ptr, rewriter);
						}
					}

					//give the record a reference to an instance of this new cold_data_struct
					coop::src_mod::add_cpr_ref_to(
						&cpr,
						&rewriter);
					
					//modify/create the record's destructor, to handle free-list fragmentation
					coop::src_mod::handle_free_list_fragmentation(
						&cpr,
						&rewriter);

					//if this record is instantiated on the heap by using new anywhere, we
					//should make sure, that the single instances are NOT distributed in memory but rather be allocated wit spatial locality
					{
						auto iter = coop::FindInstantiations::instantiations_map.find(rec.record);
						if(iter != coop::FindInstantiations::instantiations_map.end()){
							//this record is apparently being instantiated on the heap!
							//make sure to replace those instantiations with something cachefriendly
							auto &instantiations = iter->second;
							for(auto expr_contxt : instantiations){
								coop::src_mod::handle_new_instantiation(&cpr, expr_contxt, &rewriter);
							}
						}
					}
					//accordingly there should be calls to the delete operator (hopefully)
					//if so we also need to modify those code bits, to make sure the free list wont fragment
					//so whenever an element is deleted -> tell the freelist to make space for new data
					{
						auto iter  = coop::FindDeleteCalls::delete_calls_map.find(rec.record);
						if(iter != coop::FindDeleteCalls::delete_calls_map.end()){
							auto &deletions = iter->second;
							for(auto del_contxt : deletions){
								coop::src_mod::handle_delete_calls(&cpr, del_contxt, &rewriter);
							}
						}
					}

					//mark the record's translation unit as one, that needs to include the free_list_template implementation
					files_that_need_to_include_free_list.insert(member_registration_callback.class_file_map[rec.record].c_str());
				}
			}
			rewriter.overwriteChangedFiles();


			//if the user gave us an entry point to his/her include path -> drop the free_list_template.hpp in it
			//so it can be included by the relevant files
			if(user_include_path_root && system_state != 1)
			{
				//add the freelist implementation to the user's include structure
				{
					std::stringstream ss;
					ss << "cp src_mod_templates/" << coop_free_list_template_file_name << " "
						<< user_include_path_root << (user_include_path_root[strlen(user_include_path_root)-1] == '/' ? "" : "/") << coop_free_list_template_file_name;
					if(system(ss.str().c_str()) != 0){
						coop::logger::out("[ERROR] can't touch user files for injecting #include code");
					}
				}
				for(auto f : files_that_need_to_include_free_list){
					coop::src_mod::include_free_list_hpp(f, coop_free_list_template_file_name);
				}
			}
		coop::logger::out("applying changes to source files", coop::logger::DONE);
	}

	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::RUNNING);
		delete[] record_field_weight_average;
		delete[] record_stats;
	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::DONE);


	return 0;
}

void register_indirect_memberusage_in_loop_functioncalls(
	coop::LoopFunctionsCallback &loop_functions_callback,
	coop::FunctionRegistrationCallback &member_usage_callback)
{
	std::map<const Stmt*, coop::loop_credentials> purified_map; 
	//loop_functions_callback now carries ALL the loops, that call functions, even if those functions dont associate members
	//get rid of those entries 
	for(auto fc : loop_functions_callback.loop_function_calls){
		bool remove = true;
		//for each function in that loop, check wether it is a relevant function
		for(auto f : fc.second.funcs){
			auto iter = member_usage_callback.relevant_functions.find(f);
			if(iter != member_usage_callback.relevant_functions.end()){
				//this function is relevant to us and therefore validates the loop to be relevant
				remove = false;
				break;
			}
		}
		if(!remove){
			//put it in the new map, that the callback will receive
			purified_map[fc.first] = fc.second;
			//since this loop is relevant to us - check wether it is registered yet
			//if it doesnt directly associate members it wont be registered yet
			auto iter = coop::LoopMemberUsageCallback::loops.find(fc.first);
			if(iter == coop::LoopMemberUsageCallback::loops.end()){
				//the relevant loop is NOT registered yet -> register it, by adding it to the list
				coop::LoopMemberUsageCallback::loops.insert(std::pair<const Stmt*, coop::loop_credentials>(fc.first, fc.second));
				//coop::LoopMemberUsageCallback::loops[fc.first].identifier = "TODO";
				coop::LoopMemberUsageCallback::register_loop(fc.first);
			}
		}
	}

	loop_functions_callback.loop_function_calls = purified_map;
}

void create_member_matrices(
	coop::record::record_info *record_stats,
	coop::MemberRegistrationCallback &member_registration_callback,
	coop::FunctionRegistrationCallback &member_usage_callback,
	coop::LoopFunctionsCallback &loop_functions_callback,
	coop::NestedLoopCallback &nested_loop_callback)
{

	int rec_count = 0;
	for(auto cfm : member_registration_callback.class_fields_map){
		const auto fields = &cfm.second;
		const auto rec = cfm.first;
		coop::record::record_info &rec_ref = record_stats[rec_count];
		//initializing the info struct
		rec_ref.init(rec, fields,
			&member_usage_callback.relevant_functions,
			&coop::LoopMemberUsageCallback::loops);


		//iterate over each function to fill the function-member matrix of this record_info
		fill_function_member_matrix(
			rec_ref,
			fields,
			member_usage_callback);


		//loops can be nested and therefore loop A using field 'a' can nest loop B using field 'b'
		//right now our system wont recognize loop A to indirectly use 'b', we need to recursively check for dependencies like that
		//manually

		//toplevel call -> since we're iterating a mere map that doesn't
		//understand nested loops we will need to avoid redundant calls
		//first of all we need to determine wether or not thi is actually a top_level loop (doesn't appear in anyones child list)
		for(auto parent_iter : coop::NestedLoopCallback::parent_child_map){
			const Stmt* parent = parent_iter.first;
			bool is_top_level = true;
			for(auto loop_children : coop::NestedLoopCallback::parent_child_map){
				if(parent != loop_children.first){
					for(auto c : loop_children.second){
						if(c == parent){
							//this parent appears in another loop's childrens list - it will be handled by recursion implicitly
							is_top_level = false;
							break;
						}
					}
				}
				if(!is_top_level)break;
			}
			if(is_top_level){
				recursive_loop_memberusage_aggregation(nullptr, parent);
			}
		}

		//iterate over each loop to fill the loop-member matrix of this record_info
		fill_loop_member_matrix(fields,
			rec_ref,
			loop_functions_callback);

		coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [FUNCTION/member] matrix:";
		coop::logger::out();
		rec_ref.print_func_mem_mat(coop::FunctionRegistrationCallback::function_idx_mapping);
		coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [LOOP/member] matrix before weighting:";
		coop::logger::out();
		rec_ref.print_loop_mem_mat(coop::LoopMemberUsageCallback::loops, coop::LoopMemberUsageCallback::loop_idx_mapping);
		

		coop::logger::out("weighting nested loops", coop::logger::RUNNING)++;
			nested_loop_callback.print_data();
			//We should now have record_infos with valid information on which members are used by which function/loop
			//we also have the information on which loop parents which other loops
			//our approach will be to determine the loopdepth of each variable to approximate its 'weight' or 'usage frequency'
			//and therefore it's importance to the performance of the program
			//we can't reason about which loop is most important (probably), but we can take an educated guess by saying:
			//"greater loopdepth means the chance of this field being used often is also greater"

			nested_loop_callback.traverse_parents_children(
				[&rec_ref](std::map<const clang::Stmt *, coop::loop_credentials>::iterator *p, const Stmt* child_loop){
					recursive_weighting(&rec_ref, child_loop);
				}
			);
			coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [LOOP/member] matrix after weighting:";
			coop::logger::out();
			rec_ref.print_loop_mem_mat(coop::LoopMemberUsageCallback::loops, coop::LoopMemberUsageCallback::loop_idx_mapping);

		coop::logger::depth--;
		coop::logger::out("weighting nested loops", coop::logger::DONE);
		rec_count++;
	}
}

//iterate over each function to fill the function-member matrix of a record_info
void fill_function_member_matrix(coop::record::record_info &rec_ref,
		std::vector<const FieldDecl*> *fields,
		coop::FunctionRegistrationCallback &member_usage_callback)
{
	for(auto func_mems : member_usage_callback.relevant_functions){
		const FunctionDecl* func = func_mems.first;
		std::vector<const MemberExpr*> *mems = &func_mems.second;

		int func_idx = member_usage_callback.function_idx_mapping[func];
		//iterate over each member that function uses
		for(auto mem : *mems){

			coop::logger::log_stream << "checking func '" << func->getNameAsString().c_str() << "'\thas member '"
				<< mem->getMemberDecl()->getNameAsString().c_str() << "' for record '" << rec_ref.record->getNameAsString().c_str();

			const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
			if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec_ref.record){
				coop::logger::log_stream << "' - yes";
				rec_ref.fun_mem.at(rec_ref.field_idx_mapping[child], func_idx)++;
			}else{
				coop::logger::log_stream << "' - no";
			}
			coop::logger::out();
		}
	}
}

void fill_loop_member_matrix(
	std::vector<const FieldDecl*> *fields,
	coop::record::record_info &rec_ref,
	coop::LoopFunctionsCallback &loop_functions_callback )
{
	auto loop_mems_map = &coop::LoopMemberUsageCallback::loops;
	auto &loop_idxs = coop::LoopMemberUsageCallback::loop_idx_mapping;
	for(auto loop_mems : *loop_mems_map){
		auto loop = loop_mems.first;
		auto loop_info = &coop::LoopMemberUsageCallback::loops[loop];
		int loop_idx = loop_idxs[loop];
		//iterate over each member that loop uses
		for(auto mem : loop_mems.second.member_usages){

			coop::logger::log_stream << "checking loop " << loop_info->identifier << " has member '"
				<< mem->getMemberDecl()->getNameAsString().c_str() << "' for record '" << rec_ref.record->getNameAsString().c_str();

			const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
			if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec_ref.record){
				coop::logger::log_stream << "' - yes";
				rec_ref.loop_mem.at(rec_ref.field_idx_mapping[child], loop_idx)++;
			}else{
				coop::logger::log_stream << "' - no";
			}
			coop::logger::out();
		}

		/*now watch out! there can be member usage in a loop either by direct usage or inlined functions
			BUT there can also be implicit member usage in a loop by calling a function, that uses a member
			so we also need to consider all the functions, that are being called in a loop, and check, wether or
			not they use relevant members*/
		coop::logger::depth++;
		//iterate over all functions that are called inside this loops, if there are any
		auto funcsIter = loop_functions_callback.loop_function_calls.find(loop_mems.first);
		if( funcsIter != loop_functions_callback.loop_function_calls.end()){
			auto loop_info = funcsIter->second;
			//the loop has functioncalls - are the functioncalls relevant to us?
			for(auto func : loop_info.funcs){
				coop::logger::log_stream << "checking func '" << func->getNameAsString().c_str() << "' loop calls";
				coop::logger::out()++;
				//if the function declaration contains a relevant member it must be considered
				if(std::vector<const MemberExpr*> *mems = rec_ref.isRelevantFunction(func)){
					//the function is relevant -> iterate over its memberExpr and
					//update the loop_member matrix accordingly
					for(auto mem : *mems){
						//but a function can reference members of different records, so make sure to
						//only update the ones, that are relevant to us
						int mem_idx = rec_ref.isRelevantField(mem);
						if(mem_idx > -1){
							//means field is indexed by this record -> we have a match!
							//the field belongs to this record -> go update the matrix!
							coop::logger::log_stream << "found '" << func->getNameAsString().c_str() << "' using '" 
								<< mem->getMemberDecl()->getNameAsString().c_str() << "' inside the loop " <<
									loop_info.identifier;
							coop::logger::out();

							rec_ref.loop_mem.at(mem_idx, loop_idx)++;
						}
					}
				}
				coop::logger::depth--;
			}
		}
		coop::logger::depth--;
	}
}

//will check each child_loop of a parent_loop wether or not the child has a memberusage.
//If so the memberusage will also be attributed to the parent loop
void recursive_loop_memberusage_aggregation(const Stmt* parent, const Stmt* child){
	//first thing to do is go deep -> has the child any children?
	auto loop_iter = coop::NestedLoopCallback::parent_child_map.find(child);

	if(loop_iter != coop::NestedLoopCallback::parent_child_map.end()){
		for(auto c : coop::NestedLoopCallback::parent_child_map[child]){
			recursive_loop_memberusage_aggregation(child, c);
		}
	}

	if(parent){
		//make sure this loop is even relevant to us (is registered)
		auto loops = &coop::LoopMemberUsageCallback::loops;
		auto mu_iter = loops->find(child),
			 mu_iter_parent = loops->find(parent);
		if((mu_iter != loops->end()) && (mu_iter_parent != loops->end())){


			//now make sure that all member_usages are attributed to the parent
			auto mus = &mu_iter->second.member_usages;
			auto mus_parent = &mu_iter_parent->second.member_usages;
			for(auto mu : *mus){
				if(std::find(mus_parent->begin(), mus_parent->end(), mu) == mus_parent->end()){
					coop::logger::log_stream << mu_iter_parent->second.identifier << " uses "
						<< mu->getMemberDecl()->getNameAsString().c_str() << " through " << mu_iter->second.identifier;
					coop::logger::out();
					mus_parent->push_back(mu);
				}
			}
		}
	}
}

void recursive_weighting(coop::record::record_info *rec_ref, const Stmt* loop_stmt){
	//if this child_loop is relevant to us (if it associates members)
	auto loop_idx_iter = coop::LoopMemberUsageCallback::loop_idx_mapping.find(loop_stmt);
	if(loop_idx_iter != coop::LoopMemberUsageCallback::loop_idx_mapping.end()){
		int loop_idx = (*loop_idx_iter).second;
		//update the current record's member usage statistic
		for(unsigned i = 0; i < rec_ref->fields.size(); ++i){
			//since this IS  a nested loop (child loop) we can apply some arbitrary factor to the members'weights
			rec_ref->loop_mem.at(i, loop_idx) *= field_weight_depth_factor;
		}
		//since this loop_stmt could also parent other loops -> go recursive
		auto loop_stmt_iter = coop::NestedLoopCallback::parent_child_map.find(loop_stmt);
		if(loop_stmt_iter != coop::NestedLoopCallback::parent_child_map.end()){
			//this loop nests other loops -> go find them and do the same thing over again
			for(auto child : (*loop_stmt_iter).second){
				recursive_weighting(rec_ref, child);
			}
		}
	}
}
