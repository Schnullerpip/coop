#include"data.hpp"
#include"MatchCallbacks.hpp"

using namespace clang;

//local function prototypes
void recursiveLeafSearch(coop::fl_node *node, std::vector<coop::fl_node*> node_log);
int recursiveLoopDepthDetermination(coop::fl_node *node, std::vector<coop::fl_node *> node_log);
void recursiveMemberUsageAttribution(coop::fl_node *node, std::set<coop::fl_node *> node_log);
void recursive_search_for_recursion(coop::fl_node *next, std::vector<coop::fl_node*> node_log);
void recursive_relevance_check(coop::fl_node *n, std::vector<coop::fl_node*> node_log);


namespace coop{
    global<RecordDecl> g_records;
    global<FieldDecl> g_fields;
    global<FunctionDecl> g_functions;
    global<MemberExpr> g_memExprs;
    global<Stmt> g_stmts;

    std::map<const FunctionDecl *, fl_node *>
        AST_abbreviation::function_nodes = {};

    std::map<const Stmt *, fl_node *>
        AST_abbreviation::loop_nodes = {};

    std::set<fl_node*>
        AST_abbreviation::leaf_nodes;

    void AST_abbreviation::determineRecursion()
    {
        coop::logger::out("determining recursion in AST abbreviation", coop::logger::RUNNING)++;
        for(auto fnc : coop::FunctionRegistrationCallback::relevant_functions)
        {
            auto global = coop::global<FunctionDecl>::get_global(fnc.first);
            if(!global) {
                continue;
            }
            auto iter = coop::AST_abbreviation::function_nodes.find(global->ptr);
            coop::fl_node *node = nullptr;
            if(iter == coop::AST_abbreviation::function_nodes.end())
            {
                //this node is not parented by anything - yet relevant because it associates member/s 
                //most likely to find this kind of function in library code, that doesn't have a main function
                //create a node for it on the spot
                node = new coop::fl_node(global);
            }
            else{
                node = iter->second;
            }
            recursive_search_for_recursion(node, {});
        }
        coop::logger::depth--;
        coop::logger::out("determining recursion in AST abbreviation", coop::logger::DONE);
    }


    void AST_abbreviation::reduceASTabbreviation()
    {
        coop::logger::out("reducing ASTabbreviation ", coop::logger::RUNNING)++;

        //mark the relevant nodes respectively
        auto rlvnt_functions = coop::FunctionRegistrationCallback::relevant_functions;
        for(auto f_mems : rlvnt_functions)
        {
            auto global = coop::global<FunctionDecl>::get_global(f_mems.first);
            const FunctionDecl *func = global->ptr;
            if(func){
                //if that function is a parented function - make shure its parents are marked relevant 

                //the AST abbreviation remembers child/parent relations. If we find a node for this entity it is in a relation
                //and we need to go through this
                coop::fl_node * node = nullptr;
                auto iter = coop::AST_abbreviation::function_nodes.find(func);
                if(iter == coop::AST_abbreviation::function_nodes.end())
                {
                    //this node is not parented by anything - yet relevant because it associates member/s 
                    //most likely to find this kind of function in library code, that doesn't have a main function
                    //create a node for it on the spot
                    node = coop::AST_abbreviation::function_nodes[func] = new coop::fl_node(global);
                }
                else {
                    node = iter->second;
                }
                
                recursive_relevance_check(node, {});
            }
        }
        auto rlvnt_loops = coop::LoopMemberUsageCallback::loops;
        for(auto l_cred : rlvnt_loops)
        {
            const Stmt *loop = coop::global<Stmt>::get_global(l_cred.second.identifier)->ptr;
            if(loop){ //if that loop is a parented loop - make shure its parents are makred relevant 
                coop::fl_node * node = coop::AST_abbreviation::loop_nodes[loop];
                if(node){
                    recursive_relevance_check(node, {});
                }
            }
        }

        coop::logger::depth--;
        coop::logger::out("reducing ASTabbreviation ", coop::logger::DONE);
    }

    void AST_abbreviation::determineLeafNodes()
    {
        //go down each node to find the leafes
        for(auto func_node : AST_abbreviation::function_nodes)
        {
            recursiveLeafSearch(func_node.second, {});
        }
        for(auto loop_node : AST_abbreviation::loop_nodes)
        {
            recursiveLeafSearch(loop_node.second, {});
        }
    }

    void AST_abbreviation::determineLoopDepths()
    {
        //starting at the leaf nodes, go up and count loop depth
        for(auto leaf : AST_abbreviation::leaf_nodes)
        {
            recursiveLoopDepthDetermination(leaf, {});
        }
    }

    void AST_abbreviation::attributeNestedMemberUsages()
    {
		for(auto leaf : coop::AST_abbreviation::leaf_nodes)
		{
			recursiveMemberUsageAttribution(leaf, {});
		}
    }


    void fl_node::insert_child(fl_node *child)
    {
        child->parents.insert(this);
        this->children.insert(child);
    }

}//namespace coop




//local function definitions

void recursiveLeafSearch(coop::fl_node *node, std::vector<coop::fl_node*> node_log)
{
    //TODO delete this line! only for debugging
    if(node == nullptr){coop::logger::out("node pointer is NULL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1");}

    //care for recursion - if we are walking in a circle stop
    if(!node_log.empty() && std::find(node_log.begin(), node_log.end(), node) != node_log.end())
    {
        coop::AST_abbreviation::leaf_nodes.insert(node);
        return;
    }

    node_log.push_back(node);

    if(node->children.empty())
    {
        //this is a leaf register it
        coop::AST_abbreviation::leaf_nodes.insert(node);
        return;
    }

    for(auto child_node : node->children)
    {
        recursiveLeafSearch(child_node, node_log);
    }
    return;
}

int recursiveLoopDepthDetermination(coop::fl_node *node, std::vector<coop::fl_node *> node_log)
{
    //to prevent multiple calculations through multiple independent occurances of function calls
    if(node->getDepth() > 0)
    {
        return node->getDepth();
    }

    //care for recursion - if we are walking in a circle stop
    if(!node_log.empty() && std::find(node_log.begin(), node_log.end(), node) != node_log.end())
    {
        //assign both parties of the recursion a +1 treating this instance of recursion as 1 loopDepth
        node->setDepth(node->getDepth()+1);
        coop::fl_node *parent = *(node_log.end()-1);

        if(node != parent)
            parent->setDepth(parent->getDepth()+1);

        return node->getDepth();
    }
    node_log.push_back(node);

    int greatest_parent_depth = 0;
    for(auto parent_node : node->parents)
    {
        if(int d = recursiveLoopDepthDetermination(parent_node, node_log) > greatest_parent_depth)
        {
            greatest_parent_depth = d;
        }
    }

    if(node->isLoop())
    {
        node->setDepth(1 + greatest_parent_depth);
        return node->getDepth();
    }else
    {
        return greatest_parent_depth;
    }
}

void recursiveMemberUsageAttribution(coop::fl_node *node, std::set<coop::fl_node *> node_log)
{
    //care for recursion - if we are walking in a circle stop
    if(!node_log.empty() && std::find(node_log.begin(), node_log.end(), node) != node_log.end())
    {
        return;
    }
    node_log.insert(node);


    //get the childs member_usage container
    std::vector<const MemberExpr*> *member_usages_child = nullptr;
    if(node->isLoop())
    {
        //we dont need to attribute loops' member_usages to their parents since the CLANG interface will have those listed implicitly for us
        //in this case we don't want to do anything but go on with the recursion
    }else{
        auto global_child = coop::global<FunctionDecl>::get_global(node->ID());
        member_usages_child = &coop::FunctionRegistrationCallback::relevant_functions[global_child->ptr];
    }

    //for each parent, make sure they are attributed, to the members they associate indirectly through this child
    for(auto parent : node->parents)
    {
        if(node->isFunc() && parent != node){
            //get the parents member_usage container
            std::vector<const MemberExpr*> *member_usages_parent = nullptr;
            if(parent->isLoop())
            {
                auto global_parent = coop::global<Stmt>::get_global(parent->ID());
                auto &loop_credentials_parent = coop::LoopMemberUsageCallback::loops.find(global_parent->ptr)->second;
                member_usages_parent = &loop_credentials_parent.member_usages;
            }
            else{
                auto global_parent = coop::global<FunctionDecl>::get_global(parent->ID());
                member_usages_parent = &coop::FunctionRegistrationCallback::relevant_functions[global_parent->ptr];
            }

            //attribute the childs members to the parent
            //member_usages_parent->insert(member_usages_parent->begin(), member_usages_child->begin(), member_usages_child->end());
            coop::logger::log_stream << "[ATTRIBUTING]::";
            for(auto mem : (*member_usages_child))
            {
                auto iter = std::find(member_usages_parent->begin(), member_usages_parent->end(), mem);
                if(iter == member_usages_parent->end())
                {
                    coop::logger::log_stream << mem->getMemberDecl()->getNameAsString().c_str() << " ";
                    member_usages_parent->push_back(mem);
                }
            }
            coop::logger::out();
            coop::logger::log_stream << "from " << node->ID() << " to " << parent->ID();
            coop::logger::out();
        }
        recursiveMemberUsageAttribution(parent, node_log);
    }
}

void recursive_search_for_recursion(coop::fl_node *next, std::vector<coop::fl_node*> node_log)
{
    //update the node log
    node_log.push_back(next);

    for(auto child_node : next->children)
    {
        if(!node_log.empty() && std::find(node_log.begin(), node_log.end()-1, child_node) != node_log.end()-1)
        {
            //we found a recursive call
            coop::logger::log_stream << "[RECURSION]:: " << next->ID() << " <-> " << child_node->ID();
            coop::logger::out();
            next->recursive_calls.insert(child_node);
            child_node->recursive_calls.insert(next);
            return;
        }
        else
        {
            recursive_search_for_recursion(child_node, node_log);
        }
    }
}

void recursive_relevance_check(coop::fl_node *n, std::vector<coop::fl_node*> node_log)
{
    //if we happen to be in a recursion - stop
    if(!node_log.empty() && std::find(node_log.begin(), node_log.end(), n) != node_log.end())
    {
        //recursion detected
        return;
    }

    node_log.push_back(n);

    n->makeRelevant();
    if(n->isFunc()){
        coop::FunctionRegistrationCallback::registerFunction(coop::global<FunctionDecl>::get_global(n->ID())->ptr);
    }else{
        coop::LoopMemberUsageCallback::registerLoop(coop::global<Stmt>::get_global(n->ID())->ptr, n->ID(), n->isForLoop());
    }

    for(auto pn : n->parents)
    {
        recursive_relevance_check(pn, node_log);
    }
}