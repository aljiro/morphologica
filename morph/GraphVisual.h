/*!
 * \file GraphVisual
 *
 * \author Seb James
 * \date 2020
 */
#pragma once

#ifdef __OSX__
# include <OpenGL/gl3.h>
#else
# include <GL3/gl3.h>
#endif
#include <morph/tools.h>
#include <morph/VisualDataModel.h>
#include <morph/Scale.h>
#include <morph/Vector.h>
#include <morph/VisualTextModel.h>
#include <morph/Quaternion.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <sstream>

namespace morph {

    //! What shape for the graph markers?
    enum class markerstyle
    {
        none,
        triangle,
        uptriangle,
        downtriangle,
        square,
        diamond,
        pentagon,
        hexagon,
        heptagon,
        octagon,
        circle,
        numstyles
    };

    enum class tickstyle
    {
        ticksin,
        ticksout,
        numstyles
    };

    //! Different axis styles
    enum class axisstyle
    {
        L,          // just left and bottom axis bars
        box,        // left, right, top and bottom bars, ticks only on left and bottom bars
        boxfullticks, // left, right, top and bottom bars, with ticks all round
        cross,      // a cross of bars at the zero axes
        boxcross,   // A box AND the zero axes
        numstyles
    };

    //! The attributes for graphing a single dataset
    struct DatasetStyle
    {
        //! The colour of the marker
        std::array<float, 3> markercolour = {0,0,1};
        //! marker size in model units
        float markersize = 0.03f;
        //! The markerstyle. triangle, square, diamond, downtriangle, hexagon, circle, etc
        morph::markerstyle markerstyle = markerstyle::square;
        //! A gap between the data point and the line between data points
        float markergap = 0.03f;

        //! Show lines between data points? This may become a morph::linestyle thing.
        bool showlines = true;
        //! The colour of the lines between data points
        std::array<float, 3> linecolour = {0,0,0};
        //! Width of lines between data points
        float linewidth = 0.007f;
        //! Label for the dataset's legend
        std::string datalabel = "";

        //! A setter to set both colours to the same value
        void setcolour (const std::array<float, 3>& c)
        {
            this->linecolour = c;
            this->markercolour = c;
        }
    };

    /*
     * So you want to graph some data? You have an abscissa and data. Although these
     * could provide coordinates for graphing the data, it's possible that they may be
     * wide ranging. Much better to scale the data to be in the range [0,1].
     *
     * I plan to derive from GraphVisual specialized types that mean user doesn't have
     * to set all the parameters each time. E.g. LineGraphVisual, MarkerGraphVisual,
     * FatLineGraphVisual, etc.
     */
    template <typename Flt>
    class GraphVisual : public VisualDataModel<Flt>
    {
    public:
        //! Constructor which sets just the shader program and the model view offset
        GraphVisual(GLuint sp, GLuint tsp, const Vector<float> _offset)
        {
            this->shaderprog = sp;
            this->tshaderprog = tsp;
            this->mv_offset = _offset;
            this->viewmatrix.translate (this->mv_offset);
            // In GraphVisual, colourscale is used to set colour for lines when we have multiple graphs.
            this->colourScale.do_autoscale = true;
            this->zScale.do_autoscale = true;
            this->abscissa_scale.do_autoscale = true;
            // Graphs don't rotate by default. If you want yours to, set this false in your client code.
            this->twodimensional = true;
        }

        //! Update the data for the graph, recomputing the vertices when done.
        void updatedata (const std::vector<Flt>& _abscissae, const std::vector<Flt>& _data, const size_t data_idx)
        {
            throw std::runtime_error ("updatedata needs to be implemented!");
        }

        //! Set a dataset into the graph using default styles.
        void setdata (const std::vector<Flt>& _abscissae, const std::vector<Flt>& _data)
        {
            // Construct default DatasetStyle, incrementing colours and markers in a set, nice way, then call
            DatasetStyle ds;
            morph::ColourMap<float> cm;
            switch (this->n_datasets) {
                // Add cases for n_datasets==1, 2 etc.
            case 0:
            {
                // This is the first line style and takes on default values for ds
                break;
            }
            case 1:
            {
                ds.linecolour =  {1.0, 0.0, 0.0};
                ds.markerstyle = morph::markerstyle::triangle;
                ds.markercolour = cm.convert(0.1);
                break;
            }
            case 2:
            {
                ds.linecolour =  {0.0, 0.0, 1.0};
                ds.markerstyle = morph::markerstyle::downtriangle;
                ds.markercolour = {0.0, 0.0, 0.0};
                break;
            }
            case 3:
            {
                ds.linecolour =  {0.0, 0.0, 0.0};
                ds.markerstyle = morph::markerstyle::pentagon;
                ds.markercolour = {0.0, 0.0, 0.0};
                break;
            }
            case 4:
            {
                ds.linecolour =  {0.0, 1.0, 0.0};
                ds.markerstyle = morph::markerstyle::hexagon;
                ds.markercolour = {0.0, 0.0, 0.0};
                break;
            }
            case 5:
            {
                break;
            }
            case 6:
            {
                break;
            }
            case 7:
            {
                break;
            }
            case 8:
            {
                break;
            }
            case 9:
            {
                break;
            }
            case 10:
            {
                break;
            }
            default:
            {
                // Everything else gets this:
                break;
            }
            }
            this->setdata (_abscissae, _data, ds);
        }

        //! Set a dataset into the graph. Provide abscissa and ordinate and a dataset
        //! style. The locations of the markers for each dataset are computed and stored
        //! in this->dataCoords. this->dataCoords will, in a single vector of 3D coords,
        //! hold all the datasets for the graph. GraphVisual is able to render separate
        //! lines for each dataset because the index into dataCoords for the start of
        //! each dataset is saved into this->datacoord_starts.
        void setdata (const std::vector<Flt>& _abscissae, const std::vector<Flt>& _data, const DatasetStyle& ds)
        {
            if (_abscissae.size() != _data.size()) { throw std::runtime_error ("size mismatch"); }

            size_t dsize = _data.size();
            size_t dstart = 0;

            if (this->dataCoords == (std::vector<morph::Vector<float>>*)0) {
                dstart = 0;
                this->dataCoords = new std::vector<morph::Vector<float>>(dsize, {0,0,0});
            } else {
                // Add on extra capacity to dataCoords
                dstart = this->dataCoords->size();
                this->dataCoords->resize (dstart+dsize);
            }

            // Add the data style info and the starting index for dataCoords
            this->datastyles.push_back (ds);
            this->datacoord_starts.push_back (dstart);

            // Compute the zScale and asbcissa_scale for the first added dataset only
            if (this->zScale.autoscaled == false) { this->setsize (this->width, this->height); }

            // Transfor the data into temporary containers sd and ad
            std::vector<Flt> sd (dsize, Flt{0});
            std::vector<Flt> ad (dsize, Flt{0});
            this->zScale.transform (_data, sd);
            this->abscissa_scale.transform (_abscissae, ad);

            // Now sd and ad can be used to construct dataCoords x/y. They are used to
            // set the position of each datum into dataCoords
            for (size_t i = 0; i < dsize; ++i) {
                (*this->dataCoords)[dstart+i][0] = static_cast<Flt>(ad[i]);
                (*this->dataCoords)[dstart+i][1] = static_cast<Flt>(sd[i]);
                (*this->dataCoords)[dstart+i][2] = Flt{0};
            }

            this->n_datasets++;
        }

        //! Gets the graph ready for display after client setup of public attributes is done.
        void finalize()
        {
            this->initializeVertices();
            this->postVertexInit();
        }

        //! Set the graph size, in model units.
        void setsize (float _width, float _height)
        {
            std::cout << __FUNCTION__ << " called\n";
            if (this->zScale.autoscaled == true) {
                throw std::runtime_error ("Have already scaled the data, can't set the scale now.\n"
                                          "Hint: call GraphVisual::setsize() BEFORE GraphVisual::setdata() or ::setlimits()");
            }
            this->width = _width;
            this->height = _height;

            float _extra = this->dataaxisdist * this->height;
            this->zScale.range_min = _extra;
            this->zScale.range_max = this->height - _extra;

            _extra = this->dataaxisdist * this->width;
            this->abscissa_scale.range_min = _extra;
            this->abscissa_scale.range_max = this->width - _extra;

            this->thickness *= this->width;
        }

        // Axis ranges. The length of each axis could be determined from the data and
        // abscissas for a static graph, but for a dynamically updating graph, it's
        // going to be necessary to give a hint at how far the data/abscissas might need
        // to extend.
        void setlimits (Flt _xmin, Flt _xmax, Flt _ymin, Flt _ymax)
        {
            // First make sure that the range_min/max are correctly set
            this->setsize (this->width, this->height);
            // To make the axes larger, we change the scaling that we'll apply to the
            // data (the axes are always width * height in size).
            this->zScale.compute_autoscale (_ymin, _ymax);
            this->abscissa_scale.compute_autoscale (_xmin, _xmax);
        }

        //! Set the 'object thickness' attribute (maybe used just for 'object spacing')
        void setthickness (float th) { this->thickness = th; }

    protected:
        //! Compute stuff for a graph
        void initializeVertices()
        {
            // The indices index
            VBOint idx = 0;
            this->drawAxes (idx);
            this->drawData (idx);
            this->drawTickLabels(); // from which we can store the tick label widths
            this->drawAxisLabels();
        }

        void drawData (VBOint& idx)
        {
            for (size_t dsi = 0; dsi < this->n_datasets; ++dsi) {

                size_t coords_start = this->datacoord_starts[dsi];
                size_t coords_end = (dsi == this->n_datasets-1 ? this->dataCoords->size() : this->datacoord_starts[dsi+1]);

                // Draw data
                // for (auto i : data) {...
                if (this->datastyles[dsi].markerstyle != markerstyle::none) {
                    for (size_t i = coords_start; i < coords_end; ++i) {
                        this->marker (idx, (*this->dataCoords)[i], this->datastyles[dsi]);
                    }
                }
                if (this->datastyles[dsi].showlines == true) {
                    for (size_t i = 1+coords_start; i < coords_end; ++i) {
                        // Draw tube from location -1 to location 0.
#ifdef TRUE_THREE_D_LINES
                        this->computeLine (idx, (*this->dataCoords)[i-1], (*this->dataCoords)[i], uz,
                                           this->datastyles[dsi].linecolour, this->datastyles[dsi].linecolour,
                                           this->datastyles[dsi].linewidth, this->thickness*Flt{0.7}, this->datastyles[dsi].markergap);
#else
                        if (this->datastyles[dsi].markergap > 0.0f) {
                            this->computeFlatLine (idx, (*this->dataCoords)[i-1], (*this->dataCoords)[i], uz,
                                                   this->datastyles[dsi].linecolour,
                                                   this->datastyles[dsi].linewidth, this->datastyles[dsi].markergap);
                        } else {
                            // No gaps, so draw a perfect set of joined up lines
                            if (i == 1+coords_start) {
                                // First line
                                this->computeFlatLineN (idx, (*this->dataCoords)[i-1], (*this->dataCoords)[i],
                                                        (*this->dataCoords)[i+1],
                                                        uz,
                                                        this->datastyles[dsi].linecolour,
                                                        this->datastyles[dsi].linewidth);
                            } else if (i == (coords_end-1)) {
                                // last line
                                this->computeFlatLineP (idx, (*this->dataCoords)[i-1], (*this->dataCoords)[i],
                                                        (*this->dataCoords)[i-2],
                                                        uz,
                                                        this->datastyles[dsi].linecolour,
                                                        this->datastyles[dsi].linewidth);
                            } else {
                                this->computeFlatLine (idx, (*this->dataCoords)[i-1], (*this->dataCoords)[i],
                                                       (*this->dataCoords)[i-2], (*this->dataCoords)[i+1],
                                                       uz,
                                                       this->datastyles[dsi].linecolour,
                                                       this->datastyles[dsi].linewidth);
                            }
                        }
#endif
                    }
                }
            }
        }

        //! Add the axis labels
        void drawAxisLabels()
        {
            // x axis label (easy)
            morph::VisualTextModel* lbl = new morph::VisualTextModel (this->tshaderprog, this->font, this->fontsize, this->fontres);
            morph::TextGeometry geom = lbl->getTextGeometry (this->xlabel);
            morph::Vector<float> lblpos;
            if (this->axisstyle == axisstyle::cross) {
                float _y0_mdl = this->zScale.transform_one (0);
                lblpos = { 0.9f * this->width,
                           _y0_mdl-(this->axislabelgap+geom.height()+this->ticklabelgap+this->xtick_height), 0 };
            } else {
                lblpos = {0.5f * this->width - geom.half_width(),
                          -(this->axislabelgap+this->ticklabelgap+geom.height()+this->xtick_height), 0};
            }
            lbl->setupText (this->xlabel, lblpos+this->mv_offset);
            this->texts.push_back (lbl);

            // y axis label (have to rotate)
            lbl = new morph::VisualTextModel (this->tshaderprog, this->font, this->fontsize, this->fontres);
            geom = lbl->getTextGeometry (this->ylabel);

            // Rotate label if it's long
            float leftshift = geom.width();
            float downshift = geom.height();
            if (geom.width() > 2*this->fontsize) { // rotate so shift by text height
                leftshift = geom.height();
                downshift = geom.half_width();
            }

            if (this->axisstyle == axisstyle::cross) {
                float _x0_mdl = this->abscissa_scale.transform_one (0);
                lblpos = { _x0_mdl-(this->axislabelgap+leftshift+this->ticklabelgap+this->ytick_width),
                           0.9f * this->height, 0 };
            } else {
                lblpos = { -(this->axislabelgap+leftshift+this->ticklabelgap+this->ytick_width),
                           0.5f*this->height - downshift, 0 };
            }

            if (geom.width() > 2*this->fontsize) {
                morph::Quaternion<float> leftrot;
                leftrot.initFromAxisAngle (this->uz, -90.0f);
                lbl->setupText (this->ylabel, leftrot, lblpos+this->mv_offset);
            } else {
                lbl->setupText (this->ylabel, lblpos+this->mv_offset);
            }
            this->texts.push_back (lbl);
        }

        //! Graph-specific number formatting for tick labels. Someone might want to override this
        virtual std::string graphNumberFormat (Flt num)
        {
            std::stringstream ss;
            ss << num;
            std::string s = ss.str();

            if (num > Flt{-1} && num < Flt{1} && num != Flt{0}) {
                // It's a 0.something number. Get rid of any 0 preceding a '.'
                std::string::size_type p = s.find ('.');
                if (p != std::string::npos && p>0) {
                    if (s[--p] == '0') { s.erase(p, 1); }
                }
            }

            return s;
        }

        //! Add the tick labels: 0, 1, 2 etc
        void drawTickLabels()
        {
            // Reset these members
            this->xtick_height = 0.0f;
            this->ytick_width = 0.0f;

            float x_for_yticks = 0.0f;
            float y_for_xticks = 0.0f;
            if (this->axisstyle == axisstyle::cross) {
                // Then labels go next to the zero axes
                x_for_yticks = this->abscissa_scale.transform_one (0);
                y_for_xticks = this->zScale.transform_one (0);
            }

            for (unsigned int i = 0; i < this->xtick_posns.size(); ++i) {

                // Omit the 0 for 'cross' axes (or maybe shift its position)
                if (this->axisstyle == axisstyle::cross && this->xticks[i] == 0) { continue; }

                // Expunge any '0' from 0.123 so that it's .123 and so on.
                std::string s = this->graphNumberFormat (this->xticks[i]);

                // Issue: I need the width of the text ss.str() before I can create the
                // VisualTextModel, so need a static method like this:
                morph::VisualTextModel* lbl = new morph::VisualTextModel (this->tshaderprog, this->font, this->fontsize, this->fontres);
                morph::TextGeometry geom = lbl->getTextGeometry (s);
                this->xtick_height = geom.height() > this->xtick_height ? geom.height() : this->xtick_height;
                morph::Vector<float> lblpos = {this->xtick_posns[i]-geom.half_width(), y_for_xticks-(this->ticklabelgap+geom.height()), 0};
                lbl->setupText (s, lblpos+this->mv_offset);
                this->texts.push_back (lbl);
            }
            for (unsigned int i = 0; i < this->ytick_posns.size(); ++i) {

                // Omit the 0 for 'cross' axes (or maybe shift its position)
                if (this->axisstyle == axisstyle::cross && this->yticks[i] == 0) { continue; }

                std::string s = this->graphNumberFormat (this->yticks[i]);
                morph::VisualTextModel* lbl = new morph::VisualTextModel (this->tshaderprog, this->font, this->fontsize, this->fontres);
                morph::TextGeometry geom = lbl->getTextGeometry (s);
                this->ytick_width = geom.width() > this->ytick_width ? geom.width() : this->ytick_width;
                morph::Vector<float> lblpos = {x_for_yticks-this->ticklabelgap-geom.width(), this->ytick_posns[i]-geom.half_height(), 0};
                lbl->setupText (s, lblpos+this->mv_offset);
                this->texts.push_back (lbl);
            }
        }

        void drawCrossAxes (VBOint& idx)
        {
            // Vert zero is not at model(0,0), have to get model coords of data(0,0)
            float _x0_mdl = this->abscissa_scale.transform_one (0);
            float _y0_mdl = this->zScale.transform_one (0);
            this->computeFlatLine (idx,
                                   {_x0_mdl, -(this->axislinewidth*0.5f),             -this->thickness},
                                   {_x0_mdl, this->height+(this->axislinewidth*0.5f), -this->thickness},
                                   uz, this->axiscolour, this->axislinewidth*0.7f);
            // Horz zero
            this->computeFlatLine (idx,
                                   {0,           _y0_mdl, -this->thickness},
                                   {this->width, _y0_mdl, -this->thickness},
                                   uz, this->axiscolour, this->axislinewidth*0.7f);

            for (auto xt : this->xtick_posns) {
                // Want to place lines in screen units. So transform the data units
                this->computeFlatLine (idx,
                                       {(float)xt, _y0_mdl,                      -this->thickness},
                                       {(float)xt, _y0_mdl - this->ticklength,   -this->thickness}, uz,
                                       this->axiscolour, this->axislinewidth*0.5f);
            }
            for (auto yt : this->ytick_posns) {
                this->computeFlatLine (idx,
                                       {_x0_mdl,                    (float)yt, -this->thickness},
                                       {_x0_mdl - this->ticklength, (float)yt, -this->thickness}, uz,
                                       this->axiscolour, this->axislinewidth*0.5f);
            }
        }

        //! Draw the axes for the graph
        void drawAxes (VBOint& idx)
        {
            // First, ensure that this->xtick_posns/xticks and this->ytick_posns/yticks are populated
            this->computeTickPositions();

            if (this->axisstyle == axisstyle::cross) {
                return this->drawCrossAxes (idx);
            }

            if (this->axisstyle == axisstyle::box
                || this->axisstyle == axisstyle::boxfullticks
                || this->axisstyle == axisstyle::boxcross
                || this->axisstyle == axisstyle::L) {

                // y axis
                this->computeFlatLine (idx,
                                       {0, -(this->axislinewidth*0.5f),             -this->thickness},
                                       {0, this->height + this->axislinewidth*0.5f, -this->thickness},
                                       uz, this->axiscolour, this->axislinewidth);
                // x axis
                this->computeFlatLine (idx,
                                       {0,           0, -this->thickness},
                                       {this->width, 0, -this->thickness},
                                       uz, this->axiscolour, this->axislinewidth);

                // Draw left and bottom ticks
                float tl = -this->ticklength;
                if (this->tickstyle == tickstyle::ticksin) { tl = this->ticklength; }

                for (auto xt : this->xtick_posns) {
                    // Want to place lines in screen units. So transform the data units
                    this->computeFlatLine (idx,
                                           {(float)xt, 0.0f, -this->thickness},
                                           {(float)xt, tl,   -this->thickness}, uz,
                                           this->axiscolour, this->axislinewidth*0.5f);
                }
                for (auto yt : this->ytick_posns) {
                    this->computeFlatLine (idx,
                                           {0.0f, (float)yt, -this->thickness},
                                           {tl,   (float)yt, -this->thickness}, uz,
                                           this->axiscolour, this->axislinewidth*0.5f);
                }

            }

            if (this->axisstyle == axisstyle::box
                || this->axisstyle == axisstyle::boxfullticks
                || this->axisstyle == axisstyle::boxcross) {
                // right axis
                this->computeFlatLine (idx,
                                       {this->width, -this->axislinewidth*0.5f,               -this->thickness},
                                       {this->width, this->height+(this->axislinewidth*0.5f), -this->thickness},
                                       uz, this->axiscolour, this->axislinewidth);
                // top axis
                this->computeFlatLine (idx,
                                       {0,           this->height, -this->thickness},
                                       {this->width, this->height, -this->thickness},
                                       uz, this->axiscolour, this->axislinewidth);

                // Draw top and right ticks if necessary
                if (this->axisstyle == axisstyle::boxfullticks) {
                    // Tick positions
                    float tl = this->ticklength;
                    if (this->tickstyle == tickstyle::ticksin) {
                        tl = -this->ticklength;
                    }

                    for (auto xt : this->xtick_posns) {
                        // Want to place lines in screen units. So transform the data units
                        this->computeFlatLine (idx,
                                               {(float)xt, this->height,      -this->thickness},
                                               {(float)xt, this->height + tl, -this->thickness}, uz,
                                               this->axiscolour, this->axislinewidth*0.5f);
                    }
                    for (auto yt : this->ytick_posns) {
                        this->computeFlatLine (idx,
                                               {this->width,      (float)yt, -this->thickness},
                                               {this->width + tl, (float)yt, -this->thickness}, uz,
                                               this->axiscolour, this->axislinewidth*0.5f);
                    }
                }

                if (this->axisstyle == axisstyle::boxcross) { this->drawCrossAxes (idx); }
            }
        }

        //! Generate vertices for a marker of the given style at location p
        void marker (VBOint& idx, morph::Vector<float>& p, const morph::DatasetStyle& style)
        {
            switch (style.markerstyle) {
            case morph::markerstyle::triangle:
            case morph::markerstyle::uptriangle:
            {
                this->polygonMarker (idx, p, 3, style);
                break;
            }
            case morph::markerstyle::downtriangle:
            {
                this->polygonFlattop (idx, p, 3, style);
                break;
            }
            case morph::markerstyle::square:
            {
                this->polygonFlattop (idx, p, 4, style);
                break;
            }
            case morph::markerstyle::diamond:
            {
                this->polygonMarker (idx, p, 4, style);
                break;
            }
            case morph::markerstyle::pentagon:
            {
                this->polygonMarker (idx, p, 5, style);
                break;
            }
            case morph::markerstyle::hexagon:
            {
                this->polygonMarker (idx, p, 6, style);
                break;
            }
            case morph::markerstyle::heptagon:
            {
                this->polygonMarker (idx, p, 7, style);
                break;
            }
            case morph::markerstyle::octagon:
            {
                this->polygonMarker (idx, p, 8, style);
                break;
            }
            case morph::markerstyle::circle:
            default:
            {
                this->polygonMarker (idx, p, 20, style);
                break;
            }
            }
        }

        // Create an n sided polygon with first vertex 'pointing up'
        void polygonMarker  (VBOint& idx, morph::Vector<float> p, int n, const morph::DatasetStyle& style)
        {
#ifdef TRUE_THREE_D_PUCKS
            morph::Vector<float> pend = p;
            p[2] += this->thickness*Flt{0.5};
            pend[2] -= this->thickness*Flt{0.5};
            this->computeTube (idx, p, pend, ux, uy,
                               style.markercolour, style.markercolour,
                               style.markersize*Flt{0.5}, n);
#else
            p[2] += this->thickness;
            this->computeFlatPoly (idx, p, ux, uy,
                                   style.markercolour,
                                   style.markersize*Flt{0.5}, n);
#endif
        }

        // Create an n sided polygon with a flat edge 'pointing up'
        void polygonFlattop (VBOint& idx, morph::Vector<float> p, int n, const morph::DatasetStyle& style)
        {
#ifdef TRUE_THREE_D_PUCKS
            morph::Vector<float> pend = p;
            p[2] += this->thickness*Flt{0.5};
            pend[2] -= this->thickness*Flt{0.5};
            this->computeTube (idx, p, pend, ux, uy,
                               style.markercolour, style.markercolour,
                               style.markersize*Flt{0.5}, n, morph::PI_F/(float)n);
#else
            p[2] += this->thickness;
            this->computeFlatPoly (idx, p, ux, uy,
                                   style.markercolour,
                                   style.markersize*Flt{0.5}, n, morph::PI_F/(float)n);
#endif
        }

        // Given the data, compute the ticks (or use the ones that client code gave us)
        void computeTickPositions()
        {
            if (this->manualticks == true) {
                std::cout << "Writeme: Implement a manual tick-setting scheme\n";
            } else {
                // Compute locations for ticks...
                Flt _xmin = this->abscissa_scale.inverse_one (this->abscissa_scale.range_min);
                Flt _xmax = this->abscissa_scale.inverse_one (this->abscissa_scale.range_max);
                Flt _ymin = this->zScale.inverse_one (this->zScale.range_min);
                Flt _ymax = this->zScale.inverse_one (this->zScale.range_max);
#ifdef __DEBUG__
                std::cout << "x ticks between " << _xmin << " and " << _xmax << " in data units\n";
                std::cout << "y ticks between " << _ymin << " and " << _ymax << " in data units\n";
#endif
                float realmin = this->abscissa_scale.inverse_one (0);
                float realmax = this->abscissa_scale.inverse_one (this->width);
                this->xticks = this->maketicks (_xmin, _xmax, realmin, realmax);
                realmin = this->zScale.inverse_one (0);
                realmax = this->zScale.inverse_one (this->height);
                this->yticks = this->maketicks (_ymin, _ymax, realmin, realmax);

                this->xtick_posns.resize (this->xticks.size());
                this->abscissa_scale.transform (xticks, xtick_posns);

                this->ytick_posns.resize (this->yticks.size());
                this->zScale.transform (yticks, ytick_posns);
            }
        }

        /*!
         * Auto-computes the tick marker locations (in data space) for the data range
         * rmin to rmax. realmin nd realmax gives the data range actually displayed on
         * the graph - it's the data range, plus any padding introduced by
         * GraphVisual::dataaxisdist
         */
        std::deque<Flt> maketicks (Flt rmin, Flt rmax, float realmin, float realmax)
        {
            std::deque<Flt> ticks;

            Flt range = rmax - rmin;
            // How big should the range be? log the range, find the floor, raise it to get candidate
            Flt trytick = std::pow (Flt{10.0}, std::floor(std::log10 (range)));
            Flt numticks = floor(range/trytick);
            if (numticks > 10) {
                while (numticks > 10) {
                    trytick = trytick * 2;
                    numticks = floor(range/trytick);
                }
            } else {
                while (numticks < 3) {
                    trytick = trytick * 0.5;
                    numticks = floor(range/trytick);
                }
            }
#ifdef __DEBUG__
            std::cout << "Try (data) ticks of size " << trytick << ", which makes for " << numticks << " ticks.\n";
#endif
            // Realmax and realmin come from the full range of abscissa_scale/zScale
            Flt atick = trytick;
            while (atick <= realmax) {
                ticks.push_back (atick);
                atick += trytick;
            }
            atick = trytick - trytick;
            while (atick >= realmin) {
                ticks.push_back (atick);
                atick -= trytick;
            }

            return ticks;
        }

    public:
        //! A scaling for the abscissa. I'll use zScale to scale the data values
        morph::Scale<Flt> abscissa_scale;
        //! A vector of styles for the datasets to be displayed on this graph
        std::vector<DatasetStyle> datastyles;
        //! axis features, starting with the colour for the axis box/lines
        std::array<float, 3> axiscolour = {0,0,0};
        //! The line width of the main axis bars
        float axislinewidth = 0.006f;
        //! How long should the ticks be?
        float ticklength = 0.02f;
        //! Ticks in or ticks out? Or something else?
        morph::tickstyle tickstyle = tickstyle::ticksout;
        //! What sort of axes to draw: box, cross or leftbottom
        morph::axisstyle axisstyle = axisstyle::box;
        //! Show gridlines where the tick lines are?
        bool showgrid = false;
        //! Should ticks be manually set?
        bool manualticks = false;
        //! The xtick values that should be displayed
        std::deque<Flt> xticks;
        //! The positions, along the x axis (in model space) for the xticks
        std::deque<Flt> xtick_posns;
        //! The ytick values that should be displayed
        std::deque<Flt> yticks;
        //! The positions, along the y axis (in model space) for the yticks
        std::deque<Flt> ytick_posns;
        // Default font
        morph::VisualFont font = morph::VisualFont::Vera;
        //! Font resolution - determines how textures for glyphs are generated. If your
        //! labels will be small, this should be smaller. If labels are large, then it
        //! should be increased.
        int fontres = 24;
        //! The font size is the width of an m in the chosen font, in model units
        float fontsize = 0.05;
        // might need tickfontsize and axisfontsize
        //! Gap to x axis tick labels
        float ticklabelgap = 0.05;
        //! Gap from tick labels to axis label
        float axislabelgap = 0.05;
        //! The x axis label
        std::string xlabel = "x";
        //! The y axis label
        std::string ylabel = "y";

    protected:
        //! Each time setdata is used to set a dataset, increment this number
        size_t n_datasets = 0;
        //! Each element in this container is the starting index in this->dataCoords for the given dataset
        std::vector<size_t> datacoord_starts;
        //! This is used to set a spacing between elements in the graph (markers and
        //! lines) so that some objects (like a marker) is viewed 'on top' of another
        //! (such as a line). If it's too small, and the graph is far away in the scene,
        //! then precision errors can cause colour mixing.
        float thickness = 0.002f;
        //! width is how wide the graph axes will be, in 3D model coordinates
        float width = 1.0f;
        //! height is how high the graph axes will be, in 3D model coordinates
        float height = 1.0f;
        //! What proportion of the graph width/height should be allowed as a space
        //! between the min/max and the axes? Can be 0.0f;
        float dataaxisdist = 0.04f;
        //! The axes for orientation of the graph visual, which is 2D within the 3D environment.
        morph::Vector<float> ux = {1,0,0};
        morph::Vector<float> uy = {0,1,0};
        morph::Vector<float> uz = {0,0,1};

        //! Temporary storage for the max height of the xtick labels
        float xtick_height = 0.0f;
        //! Temporary storage for the max width of the ytick labels
        float ytick_width = 0.0f;
    };

} // namespace morph
