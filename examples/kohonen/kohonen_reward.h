#pragma once
#include <iostream>
#include <morph/HexGrid.h>
#include <morph/tools.h>
#include <vector>
#include <cmath>
#include "environment.h"

template<class Flt>
class Kohonen{
private:
    morph::HexGrid *hg;
    // Environment to sample
    Environment<Flt> env;
    // Learning rate
    Flt epsilon0;
    Flt epsilon;
    // Step size
    Flt h;
    // Kernel width
    Flt sigma0;
    Flt sigma;
    // Elapsed time
    Flt t;
    // Decay epsilon
    Flt alpha;
    // Decay sigma
    Flt beta;
    // Hunger
    Flt hunger;
    // Number of neurons
    unsigned int num_neurons;
    float hextohex_d = 0.01;
    float hexspan = 2;
    std::pair<Flt, Flt> v;
    unsigned int i_min;
public:
    std::vector<Flt> r; // Neuron activations
    std::vector<std::pair<Flt, Flt>> w; // weights
    
    Kohonen(Flt epsilon0, Flt sigma0, Flt alpha, Flt beta ):h(0.01), alpha(alpha), beta(beta), epsilon0(epsilon0), sigma0(sigma0){
    }

    Environment<Flt>& getEnv(){
        return env;
    }

    morph::HexGrid *getHexGrid(){
        return hg;
    }

    void setTimeStep( Flt h ){
        this->h = h;
    }

    Flt getTimeStep(){
        return h;
    }

    void initiatize(){
        hg = new morph::HexGrid (this->hextohex_d, this->hexspan, 0, morph::HexDomainShape::Boundary);
        this->hg->setEllipticalBoundary (1.0f, 1.0f);
        // hg->setBoundaryOnOuterEdge();
        hg->computeDistanceToBoundary();
        num_neurons = hg->num();
        env.initialize();
        r.resize(num_neurons);
        fill( r.begin(), r.end(), 0.0f );
        w.resize(num_neurons);
        hunger = 0;
        epsilon = epsilon0;
        sigma = sigma0;

        // initializing weights
        int rind;
        for( int i = 0; i < w.size(); i++ ){
            rind = rand()%env.getHexGrid()->num();
            w[i] = {env.getHexGrid()->d_x[rind], env.getHexGrid()->d_y[rind]};
        }

        t = 0.0;
    }

    Flt getElapsedTime(){
        return t;
    }

    Flt distance( std::pair<Flt,Flt> a, std::pair<Flt,Flt> b ){
        Flt d = std::sqrt( std::pow(a.first - b.first, 2.0) + std::pow(a.second - b.second, 2.0));
        return d;
    }

    Flt H( Flt rx, Flt ry, Flt r0x, Flt r0y, Flt t ){
        std::pair<Flt, Flt> a(rx, ry);
        std::pair<Flt, Flt> b(r0x, r0y);
        return std::exp( -std::pow(distance(a, b),2.0)/(2.0*sigma*sigma));
    }

    Flt f( Flt v ){
        return v > 0 ? 1.0 : 0.0;
    }

    Flt norm( std::pair<Flt,Flt> v ){
        Flt n = std::sqrt(v.first*v.first + v.second*v.second);
        return n;
    }

    unsigned int minDistance( std::pair<Flt, Flt> v ){
        unsigned int i_min = 0;
        Flt d_min = 10000;

        for( int i = 0; i < w.size(); ++i ){
            Flt d = distance( w[i], v );

            if( d < d_min ){
                i_min = i;  
                d_min = d;
            }   
        }

        return i_min;
    }

    Flt getIdxDistance( int idx ){
        if( idx >= 0 )
            return distance(std::pair<Flt,Flt>(hg->d_x[idx],hg->d_y[idx]), v);
        else    
            return 10000;
    }

    void step(){
        if( hunger > 0.5 ){
            std::vector<Flt> options;
            options.push_back(getIdxDistance(hg->d_ne[i_min]));
            options.push_back(getIdxDistance(hg->d_nne[i_min]));
            options.push_back(getIdxDistance(hg->d_nnw[i_min]));
            options.push_back(getIdxDistance(hg->d_nw[i_min]));
            options.push_back(getIdxDistance(hg->d_nsw[i_min]));
            options.push_back(getIdxDistance(hg->d_nse[i_min]));
            env.step_smart( options );
        }else
            env.step();
        
        // choose sensory signal
        v = env.getSignal();
        // choose excitation center
        i_min = minDistance( v );
        hunger += h*0.01*(1 - hunger);        

        // Learning
        Flt r0x = hg->d_x[i_min], r0y = hg->d_y[i_min];
        Flt rx, ry, hrr;

        for( int i = 0; i < w.size(); i++ ){
            rx = hg->d_x[i];
            ry = hg->d_y[i];
            hrr = H( r0x, r0y, rx, ry, t );
                
            w[i].first += epsilon*hrr*(v.first - w[i].first);
            w[i].second += epsilon*hrr*(v.second - w[i].second);
        }

        float avg = 0.0;
        // Compute activations
        for( int i = 0; i < r.size(); ++i ){            
            // r[i] = std::sqrt(w[i].first*w[i].first + w[i].second*w[i].second);
            r[i] = w[i].first*v.first + w[i].second*v.second;
            avg += r[i];
            // std::cout << "r: " << r[i] << "\n";
        }

        // std::cout << "epsilon: " << epsilon << "\n";
        // std::cout << "sigma: " << sigma << "\n";
        // std::cout << "hunger: " << hunger << "\n";

        if( env.getReward() && hunger > 0.5 ){
            hunger = 0.0;
            epsilon = 0.3;
            sigma = 0.1;
        }

        epsilon += h*(-alpha*epsilon); //+ 0.01*f(hunger-0.5));
        sigma += h*(-beta*sigma); //0.001*f(hunger-0.5));
        t += h;
    }
};