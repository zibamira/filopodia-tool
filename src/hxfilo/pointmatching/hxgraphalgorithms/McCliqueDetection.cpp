#include "McCliqueDetection.h"

void McCliqueDetection::getCliquesBasic(std::vector<int> & clique, 
                                        std::vector<int> & notList,
                                        std::vector<int> & cand, 
                                        std::vector<McBitfield> & connected,
                                        int & nCliques)
{
    size_t i;
    std::vector<int> notList2, cand2;
    if ( notList.size()==0 && cand.size()==0 ) {
        nCliques++;
        // printf("clique = ");
        // for ( i=0; i<clique.size(); i++ ) {
        //     printf("%d ", clique[i]);
        // }
        // printf("\n"); fflush(stdout);
        return;
    } 
    
    while ( cand.size()>0 ) {
        clique.push_back(cand[0]);
        cand2.clear();
        for ( i=1; i<cand.size(); i++ ) {
            if ( connected[cand[0]][cand[i]] ) 
                cand2.push_back(cand[i]);
        }
        notList2.clear();
        for ( i=0; i<notList.size(); i++ ) {
            if ( connected[cand[0]][notList[i]] ) 
                notList2.push_back(notList[i]);
        }
        getCliquesBasic(clique, notList2, cand2, connected, nCliques);
        
        notList.push_back(cand[0]);
        cand.erase(cand.begin());
        clique.pop_back();
    }
}

void McCliqueDetection::getCliquesBasicImproved(std::vector<int> & clique, 
                                         std::vector<int> & notAndCand, 
                                         int endNot, int endCand,
                                         std::vector<McBitfield> & connected,
                                         int & nCliques)
{
    int i, endNot2, endCand2, cand;
    std::vector<int> notAndCand2;
    if ( endCand == 0 ) {
        nCliques++;
        // printf("clique = ");
        // for ( i=0; i<clique.size(); i++ ) {
        //     printf("%d ", clique[i]);
        // }
        // printf("\n"); fflush(stdout);
        return;
    } 
    
    while ( endNot != endCand ) {
        cand = notAndCand[endNot];
        clique.push_back(cand);
        notAndCand2.clear();
        
        endNot2 = 0;
        for ( i=0; i<endNot; i++ ) {
            if ( connected[cand][notAndCand[i]] ) {
                notAndCand2.push_back(notAndCand[i]);
                endNot2++;
            }
        }
        endCand2 = endNot2;
        for ( i=endNot+1; i<endCand; i++ ) {
            if ( connected[cand][notAndCand[i]] ) {
                notAndCand2.push_back(notAndCand[i]);
                endCand2++;
            }
        }
        getCliquesBasicImproved(clique, notAndCand2, endNot2, endCand2, 
                         connected, nCliques);

        clique.pop_back();
        endNot++;
    }
}

void McCliqueDetection::getCliques(std::vector<int> & clique, 
                                   std::vector<int> & notAndCand, 
                                   int endNot, int endCand,
                                   std::vector<McBitfield> & connected,
                                   int & nCliques,
                                   std::vector<std::vector<int> > & cliques, 
                                   int minCliqueSize)
{
    std::vector<int> notAndCand2;
    
    int i, j, endNot2, endCand2, count, pos, fixp, p, s, sel;
    int minNod = endCand;
    int nod = 0;
    
    for ( i=0; i<endCand && minNod!=0; i++ ) {
        p = notAndCand[i];
        count = 0;
        for ( j=endNot; j<endCand && count<minNod; j++ ) {
            if ( !connected[p][notAndCand[j]] ) { 
                pos = j; // position of potential candidate
                count++; // increase number of disconnections
            }
        }
        if ( count < minNod ) {
            fixp = p;
            minNod = count;
            if ( i<endNot ) {
                s = pos;
            } else {
                s = i;
                nod = 1;
            }
        }
    }

    for ( nod=minNod+nod; nod>0; nod-- ) {
        p = notAndCand[s];
        notAndCand[s] = notAndCand[endNot];
        sel = notAndCand[endNot] = p;
        notAndCand2.resize(notAndCand.size());
        endNot2 = 0;
        for ( i=0; i<endNot; i++ ) {
            if ( connected[sel][notAndCand[i]] ) {
                notAndCand2[endNot2] = notAndCand[i];
                endNot2++;
            }
        }
        endCand2 = endNot2;
        for ( i=endNot+1; i<endCand; i++ ) {
            if ( connected[sel][notAndCand[i]] ) {
                notAndCand2[endCand2] = notAndCand[i];
                endCand2++;
            }
        }
        notAndCand2.resize(endCand2);
        clique.push_back(sel);
        
        if ( endCand2 == 0 ) {
            if ( static_cast<int>(clique.size()) >= minCliqueSize ) {
                nCliques++;
                cliques.push_back(clique);
            }
        } else if ( endNot2 < endCand2 ) {
            if ( static_cast<int>(clique.size())+(endCand2-endNot2) >= minCliqueSize ) {
                getCliques(clique, notAndCand2, endNot2, endCand2, 
                    connected, nCliques, cliques, minCliqueSize);
            }
        } 
        clique.pop_back();
        endNot++;
        
        if ( nod>1 ) {
            s = endNot;
            while ( connected[fixp][notAndCand[s]] ) {
                s++;
            }
        }
    }
}

