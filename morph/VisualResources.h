/*!
 * \file
 *
 * Declares a VisualResource class to hold the information about Freetype and other
 * one-per-program resources.
 *
 * \author Seb James
 * \date November 2020
 */

#pragma once

#ifdef INIT_GLFW_IN_VISUALRESOURCES
# include <GLFW/glfw3.h>
#endif
#include <iostream>
#include <utility>
#include <stdexcept>
#include <morph/VisualCommon.h>
#include <morph/VisualFace.h>
// FreeType for text rendering
#include <ft2build.h>
#include FT_FREETYPE_H

namespace morph {

    //! Singleton resource class for morph::Visual scenes.
    class VisualResources
    {
    private:
        VisualResources() {}
        ~VisualResources()
        {
            // Clean up the faces
            for (auto f : this->faces) {
                delete &f;
            }
            // We're done with freetype
            FT_Done_FreeType (this->freetype);
            // Delete self
            delete VisualResources::pInstance;
        }

#ifdef INIT_GLFW_IN_VISUALRESOURCES
        void glfw_init()
        {
            if (!glfwInit()) { std::cerr << "GLFW initialization failed!\n"; }

            // Set up error callback
            glfwSetErrorCallback (morph::VisualResources::errorCallback);

            // See https://www.glfw.org/docs/latest/monitor_guide.html
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            float xscale, yscale;
            glfwGetMonitorContentScale(primary, &xscale, &yscale);
            std::cout << "Monitor xscale: " << xscale << ", monitor yscale: " << yscale << std::endl;

            glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 1);
#ifdef __OSX__
            glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
            glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
        }
            // Tell glfw that we'd like to do anti-aliasing.
            glfwWindowHint (GLFW_SAMPLES, 4);
#endif

        void freetype_init()
        {
            // Use of gl calls here may make it neat to set up GL/GLFW here in VisualResources.
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
            morph::gl::Util::checkError (__FILE__, __LINE__);
            if (FT_Init_FreeType (&this->freetype)) {
                std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
            }
        }

        void init()
        {
#ifdef INIT_GLFW_IN_VISUALRESOURCES
            this->glfw_init();
#endif
            this->freetype_init();
        }

        //! A pointer returned to the single instance of this class
        static VisualResources* pInstance;

        //! The collection of VisualFaces generated for this instance of the
        //! application. Create one VisualFace for each unique combination of VisualFont
        //! and fontpixels (the texture resolution)
        std::map<std::pair<morph::VisualFont, unsigned int>, morph::gl::VisualFace*> faces;

#ifdef INIT_GLFW_IN_VISUALRESOURCES
        //! An error callback function for the GLFW windowing library
        static void errorCallback (int error, const char* description)
        {
            std::cerr << "Error: " << description << " (code "  << error << ")\n";
        }
#endif

    public:
        //! FreeType library object, public for access by client code
        FT_Library freetype;

        //! The instance public function. Uses the very short name 'i' to keep code tidy.
        static VisualResources* i()
        {
            if (VisualResources::pInstance == 0) {
                VisualResources::pInstance = new VisualResources;
                VisualResources::i()->init();
            }
            return VisualResources::pInstance;
        }

        //! Return a pointer to a VisualFace for the given \a font at the given texture resolution, \a fontpixels.
        morph::gl::VisualFace* getVisualFace (morph::VisualFont font, unsigned int fontpixels)
        {
            morph::gl::VisualFace* rtn = (morph::gl::VisualFace*)0;
            std::pair<morph::VisualFont, unsigned int> key = std::make_pair(font, fontpixels);
            try {
                rtn = this->faces.at (key);
            } catch (const std::out_of_range& e) {
                this->faces[key] = new morph::gl::VisualFace (font, fontpixels, this->freetype);
                rtn = this->faces.at (key);
            }
            return rtn;
        }
    };

    //! Globally initialise instance pointer to NULL
    VisualResources* VisualResources::pInstance = 0;

} // namespace morph
