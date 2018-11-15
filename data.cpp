#include"data.hpp"

using namespace clang;

namespace {
    void recursiveLeafSearch(coop::fl_node *node, std::vector<coop::fl_node*> node_log)
    {
        //TODO delete this line! only for debugging
        if(node == nullptr){coop::logger::out("node pointer is NULL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1");}

        //care for recursion - if we are walking in a circle stop
        if(!node_log.empty() && std::find(node_log.begin(), node_log.end(), node) != node_log.end())
        {
            coop::fl_node::leaf_nodes_func.insert(node);
            return;
        }

        node_log.push_back(node);

        if(node->children.empty())
        {
            //this is a leaf register it
            if(node->isFunc())
            {
                coop::fl_node::leaf_nodes_func.insert(node);
            }else{
                coop::fl_node::leaf_nodes_loop.insert(node);
            }
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
}


namespace coop{
    global<RecordDecl> g_records;
    global<FieldDecl> g_fields;
    global<FunctionDecl> g_functions;
    global<MemberExpr> g_memExprs;
    global<Stmt> g_stmts;

    std::map<const FunctionDecl *, fl_node *>
        fl_node::AST_abbreviation_func = {};

    std::map<const Stmt *, fl_node *>
        fl_node::AST_abbreviation_loop = {};

    std::set<fl_node*>
        fl_node::leaf_nodes_func;
        
    std::set<fl_node*>
        fl_node::leaf_nodes_loop;

    void fl_node::determineLeafNodes()
    {
        //go down each node to find the leafes
        for(auto func_node : fl_node::AST_abbreviation_func)
        {
            recursiveLeafSearch( func_node.second, {});
        }
        for(auto loop_node : fl_node::AST_abbreviation_loop)
        {
            recursiveLeafSearch( loop_node.second, {});
        }
    }

    void fl_node::determineLoopDepths()
    {
        //starting at the leaf nodes, go up and count loop depth
        for(auto leaf : fl_node::leaf_nodes_loop)
        {
            recursiveLoopDepthDetermination(leaf, {});
        }
        for(auto leaf : fl_node::leaf_nodes_func)
        {
            recursiveLoopDepthDetermination(leaf, {});
        }
    }


    void fl_node::insert_child(fl_node *child)
    {
        child->parents.insert(this);
        this->children.insert(child);
    }

}//namespace coop