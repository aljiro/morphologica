/*
 * Visualize a quiver field
 */
#include <morph/Visual.h>
#include <morph/ColourMap.h>
#include <morph/QuiverVisual.h>
#include <morph/GraphVisual.h>
#include <morph/Vector.h>
#include <morph/vVector.h>
#include <iostream>
#include <array>
#include <stdexcept>
#include <string>

#define G2 1

int main (int argc, char** argv)
{
    int rtn = -1;

    // Demonstrates use of offset (left at 0,0,0), lengths (3,2,1) and the 'thickness'
    // scaling factor (0.5) for the coordinate arrows
    morph::Visual v(1024, 768, "Visualization", {0.8,-0.8}, {.1,.05,.05}, 3.0f, 0.01f);
    v.showCoordArrows = true;
    v.backgroundWhite();
    v.lightingEffects();

#ifdef G2
    // second Visual
    morph::Visual v2(768, 768, "Graphs", {0.8,-0.8}, {.05,.05,.1}, 2.0f, 0.01f);
    v2.showCoordArrows = true;
    v2.backgroundWhite();
    v2.lightingEffects();
#endif
    bool holdVis = false;
    if (argc > 1) {
        std::string a1(argv[1]);
        if (a1.size() > 0) {
            holdVis = true;
        }
    }
    std::cout << "NB: Provide a cmd line arg (anything) to see the graphical windows for this program\n";

    try {
        morph::Vector<float, 3> offset = { 0.0, 0.0, 0.0 };

        std::vector<morph::Vector<float, 3>> coords;
        coords.push_back ({0, 0,   0});
        coords.push_back ({1, 1,   0});
        coords.push_back ({2, 0,   0});
        coords.push_back ({1, 0.8, 0});
        coords.push_back ({2, 0.5, 0});

        std::vector<morph::Vector<float, 3>> quivs;
        quivs.push_back ({0.3,   0.4,  0});
        quivs.push_back ({0.1,   0.2,  0.1});
        quivs.push_back ({-0.1,  0,    0});
        quivs.push_back ({-0.04, 0.05, -.2});
        quivs.push_back ({0.3,  -0.1,  0});

        std::cout << "Adding QuiverVisual to Visual 1" << std::endl;
        v.setCurrent();
        unsigned int visId = v.addVisualModel (new morph::QuiverVisual<float> (v.shaderprog, &coords, offset, &quivs, morph::ColourMapType::Cividis));

        std::cout << "Added Visual with visId " << visId << std::endl;

#ifdef G2
        // Set up v2 with a graph
        v2.setCurrent();
        morph::GraphVisual<float>* gv = new morph::GraphVisual<float> (v2.shaderprog, v2.tshaderprog, {0,0,0});
        morph::vVector<float> x =  {-.5, -.4, -.3, -.2, -.1, 0, .1, .2, .3, .4, .5, .6, .7, .8};
        morph::vVector<float> y = x.pow(3);
        gv->setdata (x, y);
        gv->finalize();
        v2.addVisualModel (static_cast<morph::VisualModel*>(gv));
#endif
        v.render();
#ifdef G2
        v2.render();
#endif
        if (holdVis == true) {
            while (v.readyToFinish == false
#ifdef G2
                   && v2.readyToFinish == false
#endif
                ) {
                glfwWaitEventsTimeout (0.018);
                v.render();
#ifdef G2
                v2.render();
#endif
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        rtn = -1;
    }

    return rtn;
}
