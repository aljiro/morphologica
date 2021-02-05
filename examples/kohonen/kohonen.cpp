#include <morph/Config.h>
#include <morph/Visual.h>
#include <morph/HexGridVisual.h>
#include <string>
#include "kohonen.h"


#ifndef FLT
# error "Please define FLT as float or double when compiling (hint: See CMakeLists.txt)"
#endif

int main( int argc, char** args ){
    bool running = true;

    if (argc < 2) {
        std::cerr << "Usage: " << args[0] << " /path/to/params.json [/path/to/logdir]" << std::endl;
        return 1;
    }

    // Loading configuration
    std::string pfile = args[1];
    morph::Config conf(pfile);

    if( !conf.ready ){
        std::cerr << "Failed reading config\n";
        return 1;
    }

    FLT epsilon = conf.getFloat( "epsilon", 0.1f );
    FLT sigma = conf.getFloat( "sigma", 1.0f );
    FLT timeStep = conf.getFloat( "timeStep", 0.01f );
    FLT maxTime = conf.getFloat( "maxTime", 1.0f );
    FLT alpha = conf.getFloat( "alpha", 0.001f );
    FLT beta = conf.getFloat( "beta", 0.01f );
    unsigned int width = conf.getUInt( "width", 800 );
    unsigned int height = conf.getUInt( "height", 640 );
    
    // Model
    Kohonen<FLT> kmap( epsilon, sigma, alpha, beta );
    kmap.setTimeStep( timeStep );
    kmap.initiatize();

    // Visual elements
    std::cout << "Creating visual model\n";
    morph::Visual plt (width, height, "Kohonen SOM");
    plt.zNear = 0.001;
    plt.zFar = 50;
    plt.fov = 45;
    plt.showCoordArrows = false;
    plt.showTitle = false;
    // You can lock movement of the scene
    plt.sceneLocked = conf.getBool ("sceneLocked", false);
    // You can set the default scene x/y/z offsets
    plt.setZDefault (conf.getFloat ("z_default", -10.0f));
    plt.setSceneTransXY(conf.getFloat ("x_default", 0.0f),
                        conf.getFloat ("y_default", 0.0f));
    // Make this larger to "scroll in and out of the image" faster
    plt.scenetrans_stepsize = 0.5;

    // Hexgrid
    morph::Vector<float, 3> spatOff = {0,0,0};
    float _m = 1.0;
    float _c = 0.0;
    morph::Scale<FLT, float> cscale;
    cscale.setParams (_m, _c);
    spatOff[0] -= 0.5 * kmap.getHexGrid()->width();
    morph::HexGridVisual<FLT>* hgv = new morph::HexGridVisual<FLT> (plt.shaderprog, plt.tshaderprog, kmap.getHexGrid(), spatOff);
    hgv->zScale.do_autoscale = false;
    hgv->colourScale.do_autoscale = false;
    hgv->setScalarData(&kmap.r);
    hgv->zScale.setParams (0.0f, 0.0f);
    hgv->addLabel("Topographic map", {-0.6f, kmap.getHexGrid()->width()/2.0f, 0},
                    morph::colour::white, morph::VisualFont::Vera, 0.12f, 64);
    hgv->setCScale (cscale);
    hgv->cm.setType (morph::ColourMapType::Inferno);
    hgv->hexVisMode = morph::HexVisMode::Triangles;
    hgv->finalize();
    
    spatOff[0] *= -1;
    morph::HexGridVisual<FLT>* hgv_a = new morph::HexGridVisual<FLT> (plt.shaderprog, plt.tshaderprog, kmap.getEnv().getHexGrid(), spatOff);
    hgv_a->zScale.do_autoscale = false;
    hgv_a->colourScale.do_autoscale = false;
    hgv_a->setScalarData(&kmap.getEnv().data);
    hgv_a->zScale.setParams (0.0, 0.0);
    hgv_a->setCScale (cscale);
    hgv_a->addLabel ("Agent's path", {-0.2f, kmap.getEnv().getHexGrid()->width()/2.0f, -0.9f},
                    morph::colour::white, morph::VisualFont::Vera, 0.12f, 64);
    hgv_a->cm.setType (morph::ColourMapType::Inferno);
    hgv_a->hexVisMode = morph::HexVisMode::Triangles;
    hgv_a->finalize();

    unsigned int n_idx = plt.addVisualModel( hgv );
    unsigned int a_idx = plt.addVisualModel( hgv_a );

    while( running ){
        kmap.step();
        hgv->updateData(&kmap.r);        
        hgv_a->updateData(&kmap.getEnv().data);  

        if( kmap.getElapsedTime() > maxTime ) running = false;

        // Render
        glfwPollEvents();

        if( static_cast<int>(kmap.getElapsedTime()/kmap.getTimeStep()) % 10 == 0 ){    
            plt.render();
        }
    }

    std::cout << "Press x in graphics window to exit.\n";
    plt.keepOpen();
    return 0;
}