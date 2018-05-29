//custom includes
#include "coop_utils.hpp"
#include "SystemStateInformation.hpp"
#include "MatchCallbacks.hpp"
//custom needed
#include "clang/AST/DeclCXX.h"

using namespace clang::tooling;
using namespace llvm;

using namespace clang;
using namespace clang::ast_matchers;



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
void recursive_weighting(coop::record::record_info *rec_info, const Stmt* child_loop);
void consider_indirect_memberusage_in_loop_functioncalls(
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
		coop::FunctionRegistrationCallback &member_usage_callback);
void fill_loop_member_matrix(
	std::vector<const FieldDecl*> *fields,
	coop::record::record_info &rec_ref,
	coop::LoopFunctionsCallback &loop_functions_callback);


//main start
int main(int argc, const char **argv) {
	//setup
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::RUNNING)++;
	coop::system::cache_credentials cc;
		coop::logger::out("retreiving system information", coop::logger::RUNNING)++;
			cc = coop::system::get_d_cache_info(coop::system::IDX_0);
			coop::logger::log_stream
				<< "for cache lvl: " << cc.lvl
				<< " size: " << cc.size
				<< "KB lineSize: " << cc.line_size << "B";
			coop::logger::out()--;
		coop::logger::out("retreiving system information", coop::logger::DONE);
		//registering all the user specified files
		std::vector<const char*> user_files;
		for(int i = 1; i < argc; ++i){
			if(!strcmp(argv[i], "--"))
				break;
			coop::logger::log_stream << "adding " << argv[i] << " to user source files";
			coop::logger::out();
			user_files.push_back(argv[i]);
		}

		CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
		ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
		MatchFinder data_aggregation;

		coop::MemberRegistrationCallback member_registration_callback(&user_files);
		coop::FunctionRegistrationCallback member_usage_callback(&user_files);
		coop::LoopFunctionsCallback loop_functions_callback(&user_files);
		coop::LoopMemberUsageCallback for_loop_member_usages_callback(&user_files);
		coop::LoopMemberUsageCallback while_loop_member_usages_callback(&user_files);
		coop::NestedLoopCallback nested_loop_callback(&user_files);

		data_aggregation.addMatcher(coop::match::members, &member_registration_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_functions, &member_usage_callback);
		data_aggregation.addMatcher(coop::match::function_calls_in_loops, &loop_functions_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_for_loops, &for_loop_member_usages_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_while_loops, &while_loop_member_usages_callback);
		data_aggregation.addMatcher(coop::match::nested_loops, &nested_loop_callback);
	coop::logger::depth--;
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::DONE);

	coop::logger::out("data aggregation (parsing AST and invoking callback routines)", coop::logger::RUNNING)++;

		//run the matchers/callbacks
		std::vector<std::unique_ptr<ASTUnit>> ASTs;
		Tool.buildASTs(ASTs);
		for(unsigned i = 0; i < ASTs.size(); ++i){
			data_aggregation.matchAST(ASTs[i]->getASTContext());
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
		consider_indirect_memberusage_in_loop_functioncalls(
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
		int num_fields = rec.member_idx_mapping.size();
		for(auto fi : rec.member_idx_mapping){
			int field_idx = fi.second;
			//add up the column of the weighted loop_mem map 
			for(unsigned y = 0; y < rec.relevant_loops->size(); ++y){
				average += rec.loop_mem.at(field_idx, y);
			}
		}

		record_field_weight_average[i] = average = average/num_fields;
		coop::logger::log_stream << "record " << rec.record->getNameAsString().c_str() << "'s field weight average = " << average;
		coop::logger::out();
	}

	//with the field weight averages (FWAs) we can now narrow down on which members are hot and cold
	//hot members will stay inside the class definition
	//cold members will be transferred to a struct, that defines those members as part of it and a
	//reference to an instance of said struct will be placed in the original record's definition

	coop::logger::depth--;
	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::TODO);

	coop::logger::out("applying changes to source files", coop::logger::RUNNING);
	//TODO: apply measurements respectively, to make the target program more cachefriendly
	coop::logger::out("applying changes to source files", coop::logger::TODO);

	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::RUNNING);
	delete[] record_field_weight_average;
	delete[] record_stats;
	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::TODO);


	return 0;
}

void consider_indirect_memberusage_in_loop_functioncalls(
	coop::LoopFunctionsCallback &loop_functions_callback,
	coop::FunctionRegistrationCallback &member_usage_callback
	){
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
		if(remove){
			loop_functions_callback.loop_function_calls.erase(fc.first);
		}else {
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
}

void create_member_matrices(
	coop::record::record_info *record_stats,
	coop::MemberRegistrationCallback &member_registration_callback,
	coop::FunctionRegistrationCallback &member_usage_callback,
	coop::LoopFunctionsCallback &loop_functions_callback,
	coop::NestedLoopCallback &nested_loop_callback
	){

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
		coop::logger::out("weighting nested loops", coop::logger::TODO);
		rec_count++;
	}
}

//iterate over each function to fill the function-member matrix of a record_info
void fill_function_member_matrix(coop::record::record_info &rec_ref,
		std::vector<const FieldDecl*> *fields,
		coop::FunctionRegistrationCallback &member_usage_callback){

	for(auto func_mems : member_usage_callback.relevant_functions){
		const FunctionDecl* func = func_mems.first;
		std::vector<const MemberExpr*> *mems = &func_mems.second;

		int func_idx = member_usage_callback.function_idx_mapping[func];
		//iterate over each member that function uses
		for(auto mem : *mems){

			coop::logger::log_stream << "checking func '" << func->getNameAsString().c_str() << "'\thas member '"
				<< mem->getMemberDecl()->getNameAsString() << "' for record '" << rec_ref.record->getNameAsString().c_str();

			const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
			if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec_ref.record){
				coop::logger::log_stream << "' - yes";
				rec_ref.fun_mem.at(rec_ref.member_idx_mapping[child], func_idx)++;
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
	coop::LoopFunctionsCallback &loop_functions_callback

	){
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
				rec_ref.loop_mem.at(rec_ref.member_idx_mapping[child], loop_idx)++;
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


void recursive_weighting(coop::record::record_info *rec_ref, const Stmt* loop_stmt){
	//if this child_loop is relevant to us (if it associates members)
	auto loop_idx_iter = coop::LoopMemberUsageCallback::loop_idx_mapping.find(loop_stmt);
	if(loop_idx_iter != coop::LoopMemberUsageCallback::loop_idx_mapping.end()){
		int loop_idx = (*loop_idx_iter).second;
		//update the current record's member usage statistic
		for(unsigned i = 0; i < rec_ref->fields.size(); ++i){
			//since this IS  a nested loop (child loop) we can ass some arbitrary factor to the members'weights
			rec_ref->loop_mem.at(i, loop_idx) *= 10;
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
