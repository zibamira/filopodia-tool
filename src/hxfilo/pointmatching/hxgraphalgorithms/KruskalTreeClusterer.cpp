#include "KruskalTreeClusterer.h"

#include <algorithm>


//(McVec3f const & one, McVec3f const & two)
inline int compareEdges(Edge * const & one, Edge * const & two) 
{    
	if(one->weight<two->weight) return -1;
    if(one->weight>two->weight) return 1;
    else return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////
// Die Hauptfunktion welche ein Bild erhaelt und es segmentiert
std::vector<std::vector<int> > KruskalTreeClusterer::clusterGraph(std::vector<std::vector<Node *> > theNodes, std::vector<Edge *> theEdges) {
    size_t i;
	// Graphen segmentieren
	std::vector<TreeList *> result = getMinimalSpanningTree(theEdges);

	std::vector<std::vector< int > > ret;
	ret.resize(theNodes.size());
    for(i=0; i < ret.size(); i++) {
        ret[i].assign(theNodes[i].size(), -1);
	}

	int x, y;
	int idx;
	int w=ret.size();

	for(i=0; i<result.size(); i++) {

		TreeList * tree = result[i];
        for(size_t e=0; e<tree->indices.size(); e++) {
			idx = tree->indices[e];
			if(idx!=0) {
				x = idx%w;
				y = idx/w;
			} else {
				x = 0;
				y = 0;
			}

			ret[x][y] = i+1;
		}
	}				

	return ret;
}
//////////////////////////////////////////////////////////////////////////////////////////
std::vector<TreeList *> KruskalTreeClusterer::getMinimalSpanningTree(std::vector<Edge *> edges) {

    size_t i=0;
	// erste Mal Menge aller Kanten dem Gewicht nach sortieren
	//sortEdges(edges);		
    std::sort(edges.begin(), edges.end());

	double sum =0;
	for(i=0; i<edges.size();i++) {		
		sum += edges[i]->weight;
	}

	sum /= edges.size();

	splitter = sum*3.0/4.0;	

	// Kruskalalgorithmus ausfuehren
	std::vector<TreeList *> trees = spanTree(edges);							

	// mutig aber richtig!		
	for(i=trees.size()-1; i>=0; i--) {
		if(trees[i]->depth < 5) {			
            trees.erase(trees.begin() + i);
		}
	}	

	return trees;
}
//////////////////////////////////////////////////////////////////////////////////////////
// fuehrt den Algorithmus von Kruskal aus
std::vector<TreeList *> KruskalTreeClusterer::spanTree(std::vector<Edge *> edges) {
	// hier werden alle bisherigen Baeume gespeichert
	// dieser Vektor muesste am Ende des Algorithmus nur noch ein Element
	// haben, der letztendliche Baum
	std::vector<TreeList *> tmpTrees;

	// die Baeume der from und to Knoten der aktuellen Kante
	TreeList * fromTree;
	TreeList * toTree;		
    for(size_t i=0; i<edges.size(); i++) {
		//	System.out.print("-"+i);
		if(edges[i]->weight>splitter) 
			break;

		// die Baeume zu denen beide Kantenpunkte hinzugehoeren
		int idxFrom = getIndexOfNode(tmpTrees, edges[i]->from);
		int idxTo = getIndexOfNode(tmpTrees, edges[i]->to);		

		// union-find machen
		if(idxFrom!=idxTo) {				
			if(idxFrom == -1) {
				// idxFrom = -1 also idxTo nicht 0, nur addEdge an ToBaum
				toTree = tmpTrees[idxTo];
				toTree->appendEdge(edges[i]);
			} else if(idxTo == -1) {
				// idxTo = -1 also idxfrom nicht 0, nur addEdge an FromBaum
				fromTree = tmpTrees[idxFrom];
				fromTree->appendEdge(edges[i]);					
			} else {
				// groesseren Baum suchen, daran den anderen ranhaengen
				// und diesen anschliessend aus dem Vector entfernen
				fromTree = tmpTrees[idxFrom];										
				toTree = tmpTrees[idxTo];

				if(fromTree->depth>toTree->depth) {
					fromTree->joinTrees(toTree);
                    tmpTrees.erase(tmpTrees.begin() + idxTo);
				} else {
					toTree->joinTrees(fromTree);
                    tmpTrees.erase(tmpTrees.begin() + idxFrom);
				}					
			}
		} else {
			// beide haben den selben Index, nun noch pruefen ob beide neu sind
			// oder ob beide im gleichen Baum sind								
			if(idxFrom==-1) {
				// beide sind neu, also neuen TreeList initiieren
				TreeList * newList = new TreeList(edges[i]);
                tmpTrees.push_back(newList);
			} else {
				// beide gehoeren zum selben Baum, also diesen Knoten nicht
				// hinzunehmen ==> nichts machen
			}
		}						
	}

	return tmpTrees;
}
//////////////////////////////////////////////////////////////////////////////////////////
// sucht in allen bisherigen Baeumen ob dieser Knoten darin vorkam und gibt
// den Index des Baumes zurueck bei dem es vorkam
int KruskalTreeClusterer::getIndexOfNode(std::vector<TreeList *> tmpTrees, Node * n) {	
    for(size_t i=0; i<tmpTrees.size(); i++) {
		if(findEdge(tmpTrees[i], n)==1) {			
			return i;
		}
	}

	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////
// sucht in einem Baum ob der Knoten darin vorkommt
int KruskalTreeClusterer::findEdge(TreeList * tList, Node * n) {
    for(size_t i=0; i<tList->indices.size(); i++) {
		if(n->index==tList->indices[i])
			return 1;
	}

	return -1;
}			


