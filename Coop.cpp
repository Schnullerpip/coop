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



//matchcallback that registeres members of classes for later usage
class MemberRegistrationCallback : public coop::CoopMatchCallback {
public:
	std::map<const RecordDecl*, std::vector<const FieldDecl*>> class_fields_map;

	MemberRegistrationCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

private:
	virtual void run(const MatchFinder::MatchResult &result){
		//retreive
		const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>(coop_class_s);

		SourceManager &srcMgr = result.Context->getSourceManager();
		if(is_user_source_file(srcMgr.getFilename(rd->getLocation()).str().c_str())){
			//register the field
			clang::RecordDecl::field_iterator fi;
			coop::logger::depth++;
			for(fi = rd->field_begin(); fi != rd->field_end(); fi++){
				class_fields_map[rd].push_back(*fi);
				coop::logger::log_stream << "found '" << fi->getNameAsString().c_str() << "' in record '" << rd->getNameAsString().c_str() << "'";
				coop::logger::out();
			}
			coop::logger::depth--;
		}
	}
};

//will cache the functions, that are matched on for later usage
class MemberUsageCallback : public coop::CoopMatchCallback{
public:
	//will hold all the functions, that use members and are therefore 'relevant' to us
	std::map<const FunctionDecl*, std::vector<const MemberExpr*>> relevant_functions;
	MemberUsageCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

private:
	void run(const MatchFinder::MatchResult &result) override {
		const FunctionDecl* func = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
		const MemberExpr* memExpr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

		SourceManager &srcMgr = result.Context->getSourceManager();
		if(is_user_source_file(srcMgr.getFilename(func->getLocation()).str().c_str())){
			coop::logger::log_stream << "found function declaration '" << func->getNameAsString() << "' using member '" << memExpr->getMemberDecl()->getNameAsString() << "'";
			coop::logger::out();

			//cache the function node for later traversal
			relevant_functions[func].push_back(memExpr);
		}
	}
};

class FunctionCallCountCallback : public coop::CoopMatchCallback {
public:
	std::map<const FunctionDecl*, int> function_number_calls;
	FunctionCallCountCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
private:
	void run(const MatchFinder::MatchResult &result){
		if(const FunctionDecl *function_call = result.Nodes.getNodeAs<CallExpr>(coop_functionCall_s)->getDirectCallee()){

			SourceManager &srcMgr = result.Context->getSourceManager();
			if(is_user_source_file(srcMgr.getFilename(function_call->getLocation()).str().c_str())){
				coop::logger::log_stream << "found function '" << function_call->getNameAsString() << "' being called";
				coop::logger::out();

				function_number_calls[function_call]++;
			}
		}
	}
};

int main(int argc, const char **argv) {

	//setup
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::RUNNING);
		int execution_state;
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

		MemberRegistrationCallback member_registration(&user_files);
		MemberUsageCallback member_usage_callback(&user_files);
		FunctionCallCountCallback function_calls_callback(&user_files);
		data_aggregation.addMatcher(coop::match::classes, &member_registration);
		data_aggregation.addMatcher(coop::match::funcs_using_members, &member_usage_callback);
		data_aggregation.addMatcher(coop::match::function_calls, &function_calls_callback);
	coop::logger::out("-----------SYSTEM SETUP-----------", coop::logger::DONE);

	coop::logger::out("determining which members/records there are", coop::logger::RUNNING)++;
		execution_state = Tool.run(newFrontendActionFactory(&data_aggregation).get());
	coop::logger::out("determining which members/records there are", coop::logger::DONE)--;

	coop::logger::out("determining which members are logically related", coop::logger::RUNNING)++;
		//creating record_info for each record
		coop::record::record_info *record_stats =
			new coop::record::record_info[member_registration.class_fields_map.size()]();
		
		coop::logger::out("creating the member matrices", coop::logger::RUNNING)++;
			//filling the info fields
			int rec_count = 0;
			for(auto pair : member_registration.class_fields_map){

				log_stream << pair.first->getNameAsString().c_str();
				coop::logger::out(log_stream)++;	
				for(auto mem : pair.second){
					coop::logger::out(mem->getNameAsString().c_str());
				}
				coop::logger::depth--;

				rec_count++;
			}

		//now we know the classes (and their members) and the functions, that use those members
		//now for each class we need to pick their member's inside the functions, to see which ones are related
		rec_count = 0;
		for(auto cfm : member_registration.class_fields_map){
			const auto fields = &cfm.second;
			const auto rec = cfm.first;
			//initializing the info struct
			record_stats[rec_count].init(rec, fields, &member_usage_callback.relevant_functions);
			//iterate over each function
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
						rec_stat->at(rec_stat->member_idx_mapping[child], func_count)++;
					}else{
						log_stream << "' - no";
					}
					coop::logger::out();
				}
				func_count++;
			}
			record_stats[rec_count++].print_mat();
		}
		coop::logger::depth--;
		coop::logger::out("creating the member matrices", coop::logger::DONE)--;
	coop::logger::out("determining which members are logically related", coop::logger::DONE);

	coop::logger::out("applying heuristic to prioritize pairings", coop::logger::RUNNING);
	//TODO: prioritize pairings
	//now that we have a matrix for each record, that tells us which of its members are used in which function
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
