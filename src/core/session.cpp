/*!
* \brief Session. 
*        最外层管理类，提供接口。
*/

// TODO: 对外提供内存管理/复用
//       对外提供多线程任务调度，开线程，ringbuffer/blocking_queue交互
//       对外提供的单函数，通过session实例进行绑定，session统一资源管理
//       

#include "ecas/ecas.hpp"

#include <iostream>
#include <vector>
#include <map>

#include "util/logger.hpp"
#include "normal_node.hpp"
#include "composite_node.hpp"
#include "tensor.hpp"
#include "topology.hpp"
#include "scheduler.hpp"

namespace ecas {

struct SessionParams {
    std::string name;
    ExecutionMode mode;
    int num_thread;

    std::map<std::string, Node*> nodes; // 包含普通节点和组合节点
    Node *input_node;
    Node *output_node;

    Topology topo;
    Scheduler scheduler; 

    SessionParams() {
        name = "noname";
        num_thread = 1;
        nodes.clear();

        input_node = nullptr;
        output_node = nullptr;
    }
    ~SessionParams() {
        for(std::map<std::string, Node*>::iterator iter = nodes.begin(); 
            iter != nodes.end(); iter++) {
            delete iter->second;
        }
        nodes.clear(); 
        // malloc_trim(0)
    }
};

Session::Session(const std::string &name, SessionConfig &config) {
    SessionParams *p = new SessionParams;
    p->name = name;
    p->mode = config.mode;
    p->num_thread = config.num_thread;
    params_ = (void *)p;
}

Session::~Session() {
    SessionParams *p = new SessionParams;
    delete (SessionParams *)params_;
}

void Session::CreateNode(const std::string &name, Task &&task, 
                         std::vector<std::vector<int>> &&input_shapes, 
                         std::vector<std::vector<int>> &&output_shapes, 
                         int group_id) {
    SessionParams *p = (SessionParams *)params_;
    Node *n = new NormalNode(name, std::forward<Task>(task), input_shapes, output_shapes);

    p->scheduler.MarkGroupId(n, group_id);
    p->nodes.insert(std::make_pair(name, n));
}
void Session::CreateNode(const std::string &name, std::vector<std::vector<std::string>> &&relation) {
    SessionParams *p = (SessionParams *)params_;
    CompositeNode *node = new CompositeNode(name, std::forward<std::vector<std::vector<std::string>>>(relation));
    p->nodes.insert(std::make_pair(name, node));
}

void Session::BuildGraph(std::vector<std::vector<std::string>> &&relation) {
    SessionParams *p = (SessionParams *)params_;
    // Build topology
    p->topo.Build(p->nodes, std::forward<std::vector<std::vector<std::string>>>(relation));
    // Specify inputs and outputs for each node according to the constructed topology.
    std::map<std::string, Node*>::iterator iter;
    for(iter = p->nodes.begin(); iter != p->nodes.end(); iter++) {
        std::vector<Node*> *inputs = p->topo.GetInputs(iter->second);
        iter->second->SetInputNodes(inputs);
        std::vector<Node*> *outputs = p->topo.GetOutputs(iter->second);
        iter->second->SetOutputNodes(outputs);
    }

    // Find the graph IO nodes according to the number of inputs and outputs.
    // Input node of the graph: no input.
    // Output node of the graph: no output.
    // Only one input node and one output node are allowed
    // Nodes with neither input nor output are not included in the graph.
    for(iter = p->nodes.begin(); iter != p->nodes.end(); iter++) {
        if (iter->second->input_nodes() == nullptr && iter->second->output_nodes() == nullptr) {
            // independent, not included in the graph
        }
        else if (iter->second->input_nodes() == nullptr) {
            if (p->input_node != nullptr) 
                ECAS_LOGE("BuildGraph -> Only one input node is allowed.\n");
            p->input_node = iter->second;
        }
        else if (iter->second->output_nodes() == nullptr) {
            if (p->output_node != nullptr) 
                ECAS_LOGE("BuildGraph -> Only one output node is allowed.\n");
            p->output_node = iter->second;
        }
    }

    // Group nodes.
    p->scheduler.UpdateGroups();
    // Check shape && allocate memory && reorder
    p->scheduler.SetupTensors(p->input_node, p->output_node);
}

void Session::Group(std::vector<std::vector<std::string>> &&groups) {
    SessionParams *p = (SessionParams *)params_;
    p->scheduler.BuildGroup(p->nodes, std::forward<std::vector<std::vector<std::string>>>(groups));
}

void Session::ShowInfo() {
    SessionParams *p = (SessionParams *)params_;

    ECAS_LOGS("\n>>>>>>>>> Session ShowInfo >>>>>>>>>\n");
    ECAS_LOGS("Session: %s.\n", p->name.c_str());
    ECAS_LOGS("Input node: %s.\n", p->input_node->name().c_str());
    ECAS_LOGS("Output node: %s.\n", p->output_node->name().c_str());

    std::map<std::string, Node*>::iterator iter;
    for(iter = p->nodes.begin(); iter != p->nodes.end(); iter++) {
        Node *n = iter->second;
        std::vector<Node *> *ins = n->input_nodes();
        std::vector<Node *> *outs = n->output_nodes();
        ECAS_LOGS("node: %s (%d) -> in: [", n->name().c_str(), n);
        for (int i = 0; i<n->input_shapes().size(); i++) {
            ECAS_LOGS("(");
            for (int j=0; j<n->input_shapes()[i].size(); j++) {
                ECAS_LOGS("%d", n->input_shapes()[i][j]);
                if (j != n->input_shapes()[i].size() - 1) ECAS_LOGS(",");
            }
            ECAS_LOGS(")");
        }
        ECAS_LOGS("], out: [");
        for (int i = 0; i<n->output_shapes().size(); i++) {
            ECAS_LOGS("(");
            for (int j=0; j<n->output_shapes()[i].size(); j++) {
                ECAS_LOGS("%d", n->output_shapes()[i][j]);
                if (j != n->output_shapes()[i].size() - 1) ECAS_LOGS(",");
            }
            ECAS_LOGS(")");
        }
        ECAS_LOGS("]\n");
    }
    ECAS_LOGS("\n");
    //
    ECAS_LOGS("Node Relationship: \n");
    for(iter = p->nodes.begin(); iter != p->nodes.end(); iter++) {
        Node *n = iter->second;
        std::vector<Node *> *ins = n->input_nodes();
        std::vector<Node *> *outs = n->output_nodes();
        ECAS_LOGS("%s -> in: [", n->name().c_str());
        if (ins != nullptr) {
            for(int i=0; i<ins->size(); i++) {
                ECAS_LOGS("%s", (*ins)[i]->name().c_str());
                if (i != ins->size() - 1) ECAS_LOGS(", ");
            }
        }
        ECAS_LOGS("], out: [");
        if (outs != nullptr) {
            for(int i=0; i<outs->size(); i++) {
                ECAS_LOGS("%s", (*outs)[i]->name().c_str());
                if (i != outs->size() - 1) ECAS_LOGS(", ");
            }
        }
        ECAS_LOGS("].\n");
    }
    ECAS_LOGS("\n");
    //
    ECAS_LOGS("Tensors: \n");
    for(iter = p->nodes.begin(); iter != p->nodes.end(); iter++) {
        Node *n = iter->second;
        std::vector<BlockingQueuePair *> ins = n->input_queues();
        std::vector<BlockingQueuePair *> outs = n->output_queues();
        if (ins.size() == 0 && outs.size() == 0)
            continue;
        ECAS_LOGS("%s -> in: [", n->name().c_str());
        for (int i = 0; i < ins.size(); i++) {
            ECAS_LOGS("%d(%s)", ins[i], ins[i]->front_name.c_str());
            if (i != ins.size() - 1)
                ECAS_LOGS(", ");
        }
        ECAS_LOGS("], out: [");
        for (int i = 0; i < outs.size(); i++) {
            ECAS_LOGS("%d(%s)", outs[i], outs[i]->rear_name.c_str());
            if (i != outs.size() - 1)
                ECAS_LOGS(", ");
        }
        ECAS_LOGS("].\n");
    }

    ECAS_LOGS("\n");
    p->scheduler.ShowGroups();
    ECAS_LOGS(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");
}

void Session::Start() {
    SessionParams *p = (SessionParams *)params_;
    // Start all task threads.
    p->scheduler.TasksSpawn();
    // p->scheduler.StartProfile();      
}

void Session::Stop() {        
    SessionParams *p = (SessionParams *)params_;
    // Stop all task threads.
    p->scheduler.TasksStop();
    p->scheduler.TasksJoin();
    ECAS_LOGI("Session::Stop().\n");
}

void Session::Feed(ITensor &in) {
    SessionParams *p = (SessionParams *)params_;
    // ECAS_LOGI("Session Running: %s, %d, %d.\n", p->name.c_str(), p->mode, p->num_thread);
    
    p->input_node->input_queues()[0]->Enqueue(in);
    // p->scheduler.BfsExecute(p->input_node, &in);
}

void Session::GetResult(ITensor *out) {
    SessionParams *p = (SessionParams *)params_;
    p->output_node->output_queues()[0]->Dequeue(out);
}

} // ecas.