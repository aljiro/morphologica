/*!
 * \file
 *
 * Declares a class to hold vertices of the quads that are the backing for a sequence of
 * text characters.
 *
 * \author Seb James
 * \date Oct 2020
 */

#pragma once

#ifdef __OSX__
# include <OpenGL/gl3.h>
#else
# include <GL3/gl3.h>
#endif
#include <morph/TransformMatrix.h>
#include <morph/Vector.h>
#include <morph/MathConst.h>
#include <vector>
#include <array>
#include <map>

// Common definitions
#include <morph/VisualCommon.h>

namespace morph {

    //! Forward declaration of a Visual class
    class Visual;

    /*!
     * A separate data-containing model which is used to render text. It is intended
     * that this could comprise part of a morph::Visual or a morph::VisualModel. It has
     * its own render call.
     */
    class VisTextModel
    {
    public:
        VisTextModel (GLuint sp, const morph::Vector<float> _offset)
        {
            // Set up...
            this->shaderprog = sp;
            this->offset = _offset;
            this->viewmatrix.translate (this->offset);

            // In derived constructor: Do the computations to initialize the vertices
            // that will represent the model
            // this->initializeVertices();

            // Then common code for postVertexInit:
            // this->postVertexInit();

            // Here's how to unbind the VAO. Is that necessary? Seems not
            // glBindVertexArray(0);
        }

        virtual ~VisTextModel()
        {
            glDeleteBuffers (4, vbos);
            delete (this->vbos);
        }

        //! With the given text and font size information, create the quads for the text.
        void setupText (const std::string& txt, std::map<char, morph::Character>& _the_characters, float fscale = 1.0f)
        {
            this->fontscale = fscale;
            // With glyph information from txt, set up this->quads.
            this->quads.clear();
            this->quad_ids.clear();
            // Our string of letters starts at this location
            float letter_pos = this->offset[0];
            for (std::string::const_iterator c = txt.begin(); c != txt.end(); c++) {
                // Add a quad to this->quads
                Character ch = _the_characters[*c];

                float xpos = letter_pos + ch.Bearing.x() * this->fontscale;
                float ypos = this->offset[1] - (ch.Size.y() - ch.Bearing.y()) * this->fontscale;
                float w = ch.Size.x() * this->fontscale;
                float h = ch.Size.y() * this->fontscale;

                // What's the order of the vertices for the quads? It is:
                // Bottom left, Top left, top right, bottom right.
                std::array<float,12> tbox = { xpos,   ypos,   this->offset[2],
                                              xpos,   ypos+h, this->offset[2],
                                              xpos+w, ypos+h, this->offset[2],
                                              xpos+w, ypos,   this->offset[2] };
#if 0
                std::cout << "Text box from (" << xpos << "," << ypos << "," << this->offset[2]
                          << ") to (" << xpos+w << "," << ypos+h << "," << this->offset[2] << ")\n";
#endif
                this->quads.push_back (tbox);
                this->quad_ids.push_back (ch.TextureID);

                // The value in ch.Advance has to be divided by 64 to bring it into the
                // same units as the ch.Size and ch.Bearing values.
                letter_pos += ((ch.Advance>>6)*this->fontscale);
            }

            // Ensure we've cleared out vertex info
            this->vertexPositions.clear();
            this->vertexNormals.clear();
            this->vertexColors.clear();

            this->initializeVertices();

            this->postVertexInit();
        }

        //! The colour of the backing quad
        std::array<float, 3> clr_backing = {0.2f, 0.2f, 0.2f};
        //! The colour of the text
        std::array<float, 3> clr_text = {1.0f, 0.0f, 0.5f};

        //! Initialize the vertices that will represent the Quads.
        void initializeVertices (void) {

            unsigned int nquads = this->quads.size();

            for (unsigned int qi = 0; qi < nquads; ++qi) {

                std::array<float, 12> quad = this->quads[qi];
                this->vertex_push (quad[0], quad[1],  quad[2],  this->vertexPositions); //1
                this->vertex_push (quad[3], quad[4],  quad[5],  this->vertexPositions); //2
                this->vertex_push (quad[6], quad[7],  quad[8],  this->vertexPositions); //3
                this->vertex_push (quad[9], quad[10], quad[11], this->vertexPositions); //4

                // Add the info for drawing the textures on the quads
                this->vertex_push (0.0f, 0.0f, 0.0f, this->vertexTextures);
                this->vertex_push (0.0f, 1.0f, 0.0f, this->vertexTextures);
                this->vertex_push (1.0f, 1.0f, 0.0f, this->vertexTextures);
                this->vertex_push (1.0f, 0.0f, 0.0f, this->vertexTextures);

                // All same colours
                this->vertex_push (this->clr_backing, this->vertexColors);
                this->vertex_push (this->clr_backing, this->vertexColors);
                this->vertex_push (this->clr_backing, this->vertexColors);
                this->vertex_push (this->clr_backing, this->vertexColors);

                // All same normals
                this->vertex_push (0.0f, 0.0f, 1.0f, this->vertexNormals);
                this->vertex_push (0.0f, 0.0f, 1.0f, this->vertexNormals);
                this->vertex_push (0.0f, 0.0f, 1.0f, this->vertexNormals);
                this->vertex_push (0.0f, 0.0f, 1.0f, this->vertexNormals);

                // Two triangles per quad
                // qi * 4 + 1, 2 3 or 4
                unsigned int ib = qi*4;
                this->indices.push_back (ib++); // 0
                this->indices.push_back (ib++); // 1
                this->indices.push_back (ib); // 2

                this->indices.push_back (ib++); // 2
                this->indices.push_back (ib);   // 3
                ib -= 3;
                this->indices.push_back (ib);   // 0
            }
        }

        //! Common code to call after the vertices have been set up.
        void postVertexInit()
        {
            std::cout << "postVertexInit...\n";
            // Create vertex array object
            glGenVertexArrays (1, &this->vao); // Safe for OpenGL 4.4-
            glBindVertexArray (this->vao);

            // Create the vertex buffer objects
            this->vbos = new GLuint[numVBO];

            glGenBuffers (numVBO, this->vbos); // OpenGL 4.4- safe

            // Set up the indices buffer - bind and buffer the data in this->indices
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vbos[idxVBO]);
            int sz = this->indices.size() * sizeof(VBOint);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sz, this->indices.data(), GL_STATIC_DRAW);

            // Binds data from the "C++ world" to the OpenGL shader world
            this->setupVBO (this->vbos[posnVBO], this->vertexPositions, posnLoc);
            this->setupVBO (this->vbos[normVBO], this->vertexNormals, normLoc);
            this->setupVBO (this->vbos[colVBO], this->vertexColors, colLoc);
            this->setupVBO (this->vbos[textureVBO], this->vertexTextures, textureLoc);

            // release (unbind) the vertex buffers
            glBindVertexArray(0);
            glBindBuffer (0, this->vbos[posnVBO]);
            glBindBuffer (0, this->vbos[normVBO]);
            glBindBuffer (0, this->vbos[colVBO]);
            glBindBuffer (0, this->vbos[textureVBO]);
            glBindBuffer (0, this->vbos[idxVBO]);

            // Possible glVertexAttribPointer and glEnableVertexAttribArray?
            glUseProgram (this->shaderprog);
        }

        //! Render the VisTextModel
        void render()
        {
            if (this->hide == true) { return; }
            glActiveTexture (GL_TEXTURE0); // sets active texture before binding vertex array
            // It is only necessary to bind the vertex array object before rendering
            glBindVertexArray (this->vao);

            // Pass this->float to GLSL so the model can have an alpha value.
            GLint loc_a = glGetUniformLocation (this->shaderprog, (const GLchar*)"alpha");
            if (loc_a != -1) { glUniform1f (loc_a, this->alpha); }

            GLint loc_tc = glGetUniformLocation (this->shaderprog, (const GLchar*)"textColour");
            if (loc_tc != -1) { glUniform3f (loc_tc, this->clr_text[0], this->clr_text[1], this->clr_text[2]); }
#if 0
            // Simple models draw all the triangles
            glDrawElements (GL_TRIANGLES, this->indices.size(), VBO_ENUM_TYPE, 0);
#endif
#if 1
            // BUT, I need to bind each texture in order. 6 indices at a time. Urgh.
            for (unsigned int i = 0; i < this->quads.size(); ++i) { // For each quad
                // render glyph texture over quad. Textures have been set up in morph::Visual.
                glBindTexture (GL_TEXTURE_2D, this->quad_ids[i]); // i
                // update content of text_vbo memory BUT already did that....
                //glBindBuffer (GL_ARRAY_BUFFER, text_vbo);
                // updates a subset of a buffer object's data store
                ///glBufferSubData (GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                // unbind
                ///glBindBuffer (GL_ARRAY_BUFFER, 0);
                // render quad
                //glDrawArrays (GL_TRIANGLES, 6*i, 6);
                glDrawElementsBaseVertex (GL_TRIANGLES, 6, VBO_ENUM_TYPE, 0, (6*i));
            }
#endif
            // Somehow get those textures on...
            glBindVertexArray(0);
        }

#if 0
        //! Temporary copy of the render text function from https://learnopengl.com/In-Practice/Text-Rendering
        void RenderText (const std::string& text, float x, float y, float scale, std::array<float, 3> color)
        {
            glUniform3f (glGetUniformLocation(this->tshaderprog, "textColor"), color[0], color[1], color[2]);

            // Set my projection.
            TransformMatrix<float> sceneview;
            sceneview.translate (this->scenetrans);
            sceneview.rotate (this->rotation);
            TransformMatrix<float> viewproj = this->projection * sceneview;

            GLint loc = glGetUniformLocation (this->shaderprog, (const GLchar*)"vp_matrix");
            if (loc != -1) { glUniformMatrix4fv (loc, 1, GL_FALSE, viewproj.mat.data());  }

            glActiveTexture (GL_TEXTURE0); // sets active texture before binding vertex array
            glBindVertexArray (this->text_vao);

            // iterate through all characters
            std::string::const_iterator c;
            for (c = text.begin(); c != text.end(); c++) {
                morph::Character ch = this->Characters[*c];

                float xpos = x + ch.Bearing.x() * scale;
                float ypos = y - (ch.Size.y() - ch.Bearing.y()) * scale;

                float w = ch.Size.x() * scale;
                float h = ch.Size.y() * scale;
                // update text_vbo for each character
                float vertices[6][4] = { // x,y ignored
                    { xpos,     ypos + h,   0.0f, 0.0f }, // tri 1
                    { xpos,     ypos,       0.0f, 1.0f },
                    { xpos + w, ypos,       1.0f, 1.0f },

                    { xpos,     ypos + h,   0.0f, 0.0f }, // tri 2. Note: I need to fill/create the texture tris.
                    { xpos + w, ypos,       1.0f, 1.0f },
                    { xpos + w, ypos + h,   1.0f, 0.0f }
                };
                // render glyph texture over quad
                glBindTexture (GL_TEXTURE_2D, ch.TextureID);
                // update content of text_vbo memory
                glBindBuffer (GL_ARRAY_BUFFER, text_vbo);
                glBufferSubData (GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glBindBuffer (GL_ARRAY_BUFFER, 0);
                // render quad
                glDrawArrays (GL_TRIANGLES, 0, 6);
                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
            }
            glBindVertexArray(0);
            glBindTexture (GL_TEXTURE_2D, 0);

            // Back to original shader
            glUseProgram (this->shaderprog);
        }
#endif

        //! The text-model-specific view matrix.
        TransformMatrix<float> viewmatrix;

    protected:

        //! Offset within the parent model or scene.
        Vector<float> offset;
        //! The Quads that form the 'medium' for the text textures. 12 float = 4 corners
        std::vector<std::array<float,12>> quads;
        //! The texture ID for each quad - so that we draw the right texture image over each quad.
        std::vector<unsigned int> quad_ids;
        //! A scaling factor for the text
        float fontscale = 1.0f;
        //! Position within vertex buffer object (if I use an array of VBO)
        enum VBOPos { posnVBO, normVBO, colVBO, idxVBO, textureVBO, numVBO };
        //! The parent Visual object - provides access to the shader prog
        const Visual* parent;
        //! A copy of the reference to the text shader program
        GLuint shaderprog;
        //! The OpenGL Vertex Array Object
        GLuint vao;
        //! Vertex Buffer Objects stored in an array
        GLuint* vbos;
        //! CPU-side data for indices
        std::vector<VBOint> indices;
        //! CPU-side data for quad vertex positions
        std::vector<float> vertexPositions;
        //! CPU-side data for quad vertex normals
        std::vector<float> vertexNormals;
        //! CPU-side data for vertex colours
        std::vector<float> vertexColors;
        //! data for textures
        std::vector<float> vertexTextures;
        //! A model-wide alpha value for the shader
        float alpha = 1.0f;
        //! If true, then calls to VisualModel::render should return
        bool hide = false;

        //! Set up a vertex buffer object - bind, buffer and set vertex array object attribute
        void setupVBO (GLuint& buf, std::vector<float>& dat, unsigned int bufferAttribPosition)
        {
            int sz = dat.size() * sizeof(float);
            glBindBuffer (GL_ARRAY_BUFFER, buf);
            glBufferData (GL_ARRAY_BUFFER, sz, dat.data(), GL_STATIC_DRAW);
            glVertexAttribPointer (bufferAttribPosition, 3, GL_FLOAT, GL_FALSE, 0, (void*)(0));
            glEnableVertexAttribArray (bufferAttribPosition);
        }

        //! Push three floats onto the vector of floats \a vp
        void vertex_push (const float& x, const float& y, const float& z, std::vector<float>& vp)
        {
            vp.push_back (x);
            vp.push_back (y);
            vp.push_back (z);
        }
        //! Push array of 3 floats onto the vector of floats \a vp
        void vertex_push (const std::array<float, 3>& arr, std::vector<float>& vp)
        {
            vp.push_back (arr[0]);
            vp.push_back (arr[1]);
            vp.push_back (arr[2]);
        }
        //! Push morph::Vector of 3 floats onto the vector of floats \a vp
        void vertex_push (const Vector<float>& vec, std::vector<float>& vp)
        {
            std::copy (vec.begin(), vec.end(), std::back_inserter (vp));
        }
    };

} // namespace morph