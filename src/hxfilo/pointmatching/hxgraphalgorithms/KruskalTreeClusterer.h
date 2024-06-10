#ifndef KRUSKAL_TREE_CLUSTERER_H
#define KRUSKAL_TREE_CLUSTERER_H

#include "api.h"
#include <math.h>
#include <vector>

class Node;
class Edge;
class TreeList;

class HXGRAPHALGORITHMS_API KruskalTreeClusterer {
public:
	// Main function: returning for each node a id of its cluster (-1 if not clustered)
	// theNodes and theEdges dexcribing the graph to be clustered
	std::vector<std::vector<int> > clusterGraph(std::vector<std::vector<Node *> > theNodes, std::vector<Edge *> theEdges);

private:

	// weight separation, edges weighted higher then splitter, wont be used
	double splitter;

	// Kruskal Tree Clustering Algorithm
	std::vector<TreeList *> getMinimalSpanningTree(std::vector<Edge *> edges);

	// fuehrt den Algorithmus von Kruskal aus
	std::vector<TreeList *> spanTree(std::vector<Edge *> edges);

	// sucht in allen bisherigen Baeumen ob dieser Knoten darin vorkam und gibt
	// den Index des Baumes zurueck bei dem es vorkam
	int getIndexOfNode(std::vector<TreeList *> tmpTrees, Node * n);

	// sucht in einem Baum ob der Knoten darin vorkommt
	int findEdge(TreeList * tList, Node * n);
};


class Node {
public:
	int x;	
	int y;
	
	int index;
	
	std::vector<double> weights;
	
	Node(int xx, int yy, int idx, std::vector<double> vals) {
		x = xx;
		y = yy;
		
		index = idx;
		
		weights = vals;
	}
};

class Edge {
public:
	Node * from;
	Node * to;
	
	double weight;
	
	Edge(Node * fr, Node * t) {
		from = fr;
		to = t;

		weight = 0;
        for(size_t i=0; i<fr->weights.size(); i++) {
			weight += ((from->weights[i]-to->weights[i])*(from->weights[i]-to->weights[i]));
		}	

		weight = sqrt(weight);

		double half = sqrt((double)fr->weights.size()); 
		if(weight>(half/2)) {
			weight = half - weight;
		}
	}
};

class TreeList {	
public:
	std::vector<int> indices;		
	std::vector<Edge *> allEdges;	
	
	int depth;	
	
	// initialisiert den Gesamten Baum
	TreeList(Edge * e) {		
        allEdges.push_back(e);
				
        indices.push_back(e->from->index);
        indices.push_back(e->to->index);
		
		depth = 1;
	}
	
	void joinTrees(TreeList * tList) {
		for(int i=0; i<tList->depth-1; i++) {
			appendEdge(tList->allEdges[i]);
		}
	}
	
	void appendEdge(Edge  *e) {
        allEdges.push_back(e);
        indices.push_back(e->from->index);
        indices.push_back(e->to->index);
		
		depth++;
	}
	
	
};

#endif



