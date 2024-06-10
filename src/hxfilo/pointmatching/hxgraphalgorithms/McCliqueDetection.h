#ifndef MC_CLIQUE_DETECTION_H
#define MC_CLIQUE_DETECTION_H

#include <mclib/McBitfield.h>

#include "api.h"

#include <vector>

class HXGRAPHALGORITHMS_API McCliqueDetection {

public:
    static void getCliquesBasic(std::vector<int> & clique, 
                           std::vector<int> & ,
                           std::vector<int> & cand, 
                           std::vector<McBitfield> & connected,
                           int & nCliques);
    
    static void getCliquesBasicImproved(std::vector<int> & clique, 
                           std::vector<int> & notAndCand, 
                           int endNot, int endCand,
                           std::vector<McBitfield> & connected,
                           int & nCliques);
    
    static void getCliques(std::vector<int> & clique, 
                           std::vector<int> & notAndCand, 
                           int endNot, int endCand,
                           std::vector<McBitfield> & connected,
                           int & nCliques,
                           std::vector<std::vector<int> > & cliques, 
                           int minCliqueSize=1);
};

#endif
