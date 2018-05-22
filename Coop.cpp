//custom includes
#include "SystemStateInformation.hpp"
#include "coop_utils.hpp"
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


int main(int argc, const char **argv) {

	//setup
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::RUNNING);
		extern int execution_state;
		std::stringstream& log_stream = coop::logger::log_stream;

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
		coop::LoopRegistrationCallback for_loop_registration_callback(&user_files);
		coop::LoopRegistrationCallback while_loop_registration_callback(&user_files);
		coop::NestedLoopCallback nested_loop_callback(&user_files);

		data_aggregation.addMatcher(coop::match::members, &member_registration_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_functions, &member_usage_callback);
		data_aggregation.addMatcher(coop::match::function_calls_in_loops, &loop_functions_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_for_loops, &for_loop_registration_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_while_loops, &while_loop_registration_callback);
		data_aggregation.addMatcher(coop::match::nested_loops, &nested_loop_callback);
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::DONE);

	coop::logger::out("data aggregation (parsing AST and invoking callback routines)", coop::logger::RUNNING)++;

		//run the matchers/callbacks
		execution_state = Tool.run(newFrontendActionFactory(&data_aggregation).get());

		//print out the found records (classes/structs) and their fields
		member_registration_callback.printData();
		coop::logger::depth--;
	coop::logger::out("data aggregation (parsing AST and invoking callback routines)", coop::logger::DONE);

	coop::logger::out("determining which members are logically related", coop::logger::RUNNING)++;
		//creating record_info for each record
		coop::record::record_info *record_stats =
			new coop::record::record_info[member_registration_callback.class_fields_map.size()]();

		coop::logger::out("creating the member matrices", coop::logger::RUNNING)++;
		//the loop_registration_callback contains all the loops that directly associate members
		//loop_functions_callback contains all the loops, that call functions and therefore might indirectly associate members
		//find out which loop_functions_callback's functions are missing and extend the loop_registration

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
				auto iter = coop::LoopRegistrationCallback::loops.find(fc.first);
				if(iter == coop::LoopRegistrationCallback::loops.end()){
					//the relevant loop is NOT registered yet -> register it, by adding it to the list
					coop::LoopRegistrationCallback::loops[fc.first].identifier = "TODO";
					coop::LoopRegistrationCallback::register_loop(fc.first);
				}
			}
		}

		/*now we know the classes (and their members) and the functions as well as all the loops, that use those members
		now for each class we need to pick their members inside the functions, to see which ones are related*/
		int rec_count = 0;
		for(auto cfm : member_registration_callback.class_fields_map){
			const auto fields = &cfm.second;
			const auto rec = cfm.first;
			coop::record::record_info &rec_ref = record_stats[rec_count];
			//initializing the info struct
			rec_ref.init(rec, fields,
				&member_usage_callback.relevant_functions,
				&coop::LoopRegistrationCallback::loops);
			//iterate over each function to fill the function-member matrix of this record_info
			{
				for(auto func_mems : member_usage_callback.relevant_functions){
					const FunctionDecl* func = func_mems.first;
					std::vector<const MemberExpr*> *mems = &func_mems.second;

					int func_idx = member_usage_callback.function_idx_mapping[func];
					//iterate over each member that function uses
					for(auto mem : *mems){

						log_stream << "checking func '" << func->getNameAsString().c_str() << "'\thas member '"
							<< mem->getMemberDecl()->getNameAsString() << "' for record '" << rec->getNameAsString().c_str();

						const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
						if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec){
							log_stream << "' - yes";
							rec_ref.fun_mem.at(rec_ref.member_idx_mapping[child], func_idx)++;
						}else{
							log_stream << "' - no";
						}
						coop::logger::out();
					}
				}
			}
			//iterate over each loop to fill the loop-member matrix of this record_info
			{
				auto loop_mems_map = &coop::LoopRegistrationCallback::loops;
				auto &loop_idxs = coop::LoopRegistrationCallback::loop_idx_mapping;
				for(auto loop_mems : *loop_mems_map){
					auto loop = loop_mems.first;
					int loop_idx = loop_idxs[loop];
					//iterate over each member that loop uses
					for(auto mem : loop_mems.second.member_usages){

						log_stream << "checking loop has member '"
							<< mem->getMemberDecl()->getNameAsString().c_str() << "' for record '" << rec->getNameAsString().c_str();

						const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
						if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec){
							log_stream << "' - yes";
							rec_ref.loop_mem.at(rec_ref.member_idx_mapping[child], loop_idx)++;
						}else{
							log_stream << "' - no";
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
				rec_count++;
			}

			coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [FUNCTION/member] matrix:";
			coop::logger::out()++;
			rec_ref.print_func_mem_mat();
			coop::logger::depth--;
			coop::logger::log_stream << rec_ref.record->getNameAsString().c_str() << "'s [LOOP/member] matrix:";
			coop::logger::out()++;
			rec_ref.print_loop_mem_mat(&for_loop_registration_callback);
			coop::logger::depth--;
		}

		coop::logger::out("checking for nested loops", coop::logger::RUNNING)++;
		//TODO
		coop::logger::depth--;
		coop::logger::out("checking for nested loops", coop::logger::TODO);

		coop::logger::depth--;
		coop::logger::out("creating the member matrices", coop::logger::DONE)--;

	coop::logger::out("determining which members are logically related", coop::logger::DONE);

	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::RUNNING);
	//now that we have a matrix for each record, that tells us which of its members are used in which function how many times,
	//we can take a heuristic and prioritize pairings
	//by determining which of the members are used most frequently together, we know which ones to make cachefriendly
	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::TODO);

	coop::logger::out("applying changes to AST", coop::logger::RUNNING);
	//TODO: apply measurements respectively, to make the target program more cachefriendly
	coop::logger::out("applying changes to AST", coop::logger::TODO);

	coop::logger::out("creating Intermediate Representation (IR)", coop::logger::RUNNING);
	coop::logger::out("creating Intermediate Representation (IR)", coop::logger::TODO);

	coop::logger::out("creating executable", coop::logger::RUNNING);
	coop::logger::out("creating executable", coop::logger::TODO);

	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::RUNNING);
	delete[] record_stats;
	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::TODO);

	return execution_state;
}
