// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ASTContext.h"

using namespace clang::tooling;
using namespace llvm;

using namespace clang;
using namespace clang::ast_matchers;

//custom includes
#include "SystemStateInformation.hpp"
#include "coop_utils.hpp"
//custom needed
#include "clang/AST/DeclCXX.h"

std::map<const clang::Stmt*, std::vector<const clang::MemberExpr*>>
coop::LoopRegistrationCallback::loop_members_map = {};



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
		coop::MemberUsageInFunctionsCallback member_usage_callback(&user_files);
		coop::LoopFunctionsCallback loop_functions_callback(&user_files);
		coop::LoopRegistrationCallback for_loop_registration_callback(&user_files);
		coop::LoopRegistrationCallback while_loop_registration_callback(&user_files);

		data_aggregation.addMatcher(coop::match::members, &member_registration_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_functions, &member_usage_callback);
		data_aggregation.addMatcher(coop::match::function_calls_in_loops, &loop_functions_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_for_loops, &for_loop_registration_callback);
		data_aggregation.addMatcher(coop::match::members_used_in_while_loops, &while_loop_registration_callback);
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::DONE);

	coop::logger::out("data aggregation (applying matchers and callback routines)", coop::logger::RUNNING)++;
		execution_state = Tool.run(newFrontendActionFactory(&data_aggregation).get());
		coop::logger::depth--;
	coop::logger::out("data aggregation (applying matchers and callback routines)", coop::logger::DONE);

	coop::logger::out("determining which members are logically related", coop::logger::RUNNING)++;
		//creating record_info for each record
		coop::record::record_info *record_stats =
			new coop::record::record_info[member_registration_callback.class_fields_map.size()]();
		
		coop::logger::out("creating the member matrices", coop::logger::RUNNING)++;
			//print out the found records (classes/structs) and their fields
			int rec_count = 0;
			for(auto pair : member_registration_callback.class_fields_map){

				coop::logger::out(pair.first->getNameAsString().c_str())++;	
				for(auto mem : pair.second){
					coop::logger::out(mem->getNameAsString().c_str());
				}
				coop::logger::depth--;

				rec_count++;
			}

		/*now we know the classes (and their members) and the functions, that use those members and
		also all the loops that use them
		now for each class we need to pick their members inside the functions, to see which ones are related*/
		rec_count = 0;
		for(auto cfm : member_registration_callback.class_fields_map){
			const auto fields = &cfm.second;
			const auto rec = cfm.first;
			//initializing the info struct
			record_stats[rec_count].init(rec, fields,
				&member_usage_callback.relevant_functions,
				&coop::LoopRegistrationCallback::loop_members_map);
			//iterate over each function to fill the function-member matrix of this record_info
			{
				int func_count = 0;
				for(auto func : member_usage_callback.relevant_functions){
					//iterate over each member that function uses
					for(auto mem : func.second){

						log_stream << "checking func '" << func.first->getNameAsString().c_str() << "'\thas member '"
							<< mem->getMemberDecl()->getNameAsString() << "' for record '" << rec->getNameAsString().c_str();

						const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
						if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec){
							log_stream << "' - yes";
							auto rec_stat = &record_stats[rec_count];
							rec_stat->fun_mem.at(rec_stat->member_idx_mapping[child], func_count)++;
						}else{
							log_stream << "' - no";
						}
						coop::logger::out();
					}
					func_count++;
				}
                coop::logger::log_stream << "record '" << record_stats[rec_count].record->getNameAsString().c_str() << "'s [FUNCTION/MEMBER] matrix:";
				coop::logger::out();
				coop::logger::depth++;
				record_stats[rec_count].print_func_mem_mat();
				coop::logger::depth--;
			}
			//iterate over each loop to fill the loop-member matrix of this record_info
			{
				int loop_count = 0;
				for(auto loop : for_loop_registration_callback.loop_members_map){
					//iterate over each member that function uses
					for(auto mem : loop.second){

						log_stream << "checking loop has member '"
							<< mem->getMemberDecl()->getNameAsString().c_str() << "' for record '" << rec->getNameAsString().c_str();

						const FieldDecl* child = static_cast<const FieldDecl*>(mem->getMemberDecl());
						if(std::find(fields->begin(), fields->end(), child)!=fields->end() && child->getParent() == rec){
							log_stream << "' - yes";
							auto rec_stat = &record_stats[rec_count];
							rec_stat->loop_mem.at(rec_stat->member_idx_mapping[child], loop_count)++;
						}else{
							log_stream << "' - no";
						}
						coop::logger::out();
					}
					loop_count++;
				}
                coop::logger::log_stream << "record '" << record_stats[rec_count].record->getNameAsString().c_str() << "'s [LOOP/MEMBER] matrix:";
				coop::logger::out();
				coop::logger::depth++;
				record_stats[rec_count++].print_loop_mem_mat();
				coop::logger::depth--;
			}
		}
		coop::logger::depth--;
		coop::logger::out("creating the member matrices", coop::logger::DONE)--;

	coop::logger::out("determining which members are logically related", coop::logger::DONE);

	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::RUNNING);
	//TODO: prioritize pairings
	//now that we have a matrix for each record, that tells us which of its members are used in which function how many times,
	//we can take a heuristic and prioritize pairings
	//by determining which of the members are used most frequently together, we know which ones to make cachefriendly
	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::TODO);

	coop::logger::out("applying changes to AST ... ", coop::logger::RUNNING);
	//TODO: apply measurements respectively, to make the target program more cachefriendly
	coop::logger::out("applying changes to AST ... ", coop::logger::TODO);

	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::RUNNING);
	delete[] record_stats;
	coop::logger::out("-----------SYSTEM CLEANUP-----------", coop::logger::TODO);

	return execution_state;
}
