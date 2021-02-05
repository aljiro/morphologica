#pragma once
#include <utility>
#include <cstdlib>


template<class Flt>
class Environment{
private:
    int agent;
    morph::HexGrid *hg;
    float hextohex_d = 0.05;
    float hexspan = 2;
public:
    std::vector<Flt> data;

    Environment(){
    }

    morph::HexGrid *getHexGrid(){
        return hg;
    }

    void initialize(){        
        hg = new morph::HexGrid (this->hextohex_d, this->hexspan, 0, morph::HexDomainShape::Boundary);
        this->hg->setEllipticalBoundary (1.0f, 1.0f);
        // hg->setBoundaryOnOuterEdge();
        hg->computeDistanceToBoundary();
        agent = rand()%hg->num();
        data.resize( hg->num() );
        std::fill( data.begin(), data.end(), 0.2 );
    }

    int selectAction( int nextp ){
        int nagent; 
        switch( nextp ){
            case 0:
                nagent = hg->d_ne[agent];
                break;
            case 1:
                nagent = hg->d_nne[agent];
                break;
            case 2:
                nagent = hg->d_nnw[agent];
                break;
            case 3:
                nagent = hg->d_nw[agent];
                break;
            case 4:
                nagent = hg->d_nsw[agent];
                break;
            case 5:
                nagent = hg->d_nse[agent];
                break;
        }

        return nagent;
    }

    void step( Flt t ){  
        if( t < 50 ){
            agent = rand()%hg->num();
            
        }else{
            int nextp, nagent;
            do{
                nextp = rand() % 6;

                nagent = selectAction(nextp);
            }while( nagent == -1 );

            agent = nagent;

            for( int i = 0; i < data.size(); ++i )
                if( data[i] > 0.15 ) data[i] -= 0.005;
        }
        data[agent] = 1.0;
    }

    std::pair<Flt, Flt> getSignal(){
        std::pair<Flt, Flt> res(hg->d_x[agent], hg->d_y[agent]);
        float n = std::sqrt(res.first*res.first + res.second*res.second);
        if( n!= 0 ){
            res.first /= n;
            res.second /= n;
        }
        return res;
    }

};