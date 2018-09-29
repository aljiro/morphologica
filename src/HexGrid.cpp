/*!
 * Implementation of HexGrid
 *
 * Author: Seb James
 *
 * Date: 2018/07
 */

#include "HexGrid.h"
#include <cmath>
#include <float.h>
#include <limits>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>
#include <stdexcept>
#include "BezCurvePath.h"
#include "BezCoord.h"

#define DBGSTREAM std::cout
#define DEBUG 1
//#define DEBUG2 1
#include "MorphDbg.h"

using std::ceil;
using std::abs;
using std::endl;
using std::stringstream;
using std::vector;
using std::set;
using std::runtime_error;
using std::numeric_limits;

using morph::BezCurvePath;
using morph::BezCoord;
using morph::Hex;

morph::HexGrid::HexGrid ()
    : d(1.0f)
    , x_span(1.0f)
    , z(0.0f)
{
}

morph::HexGrid::HexGrid (float d_, float x_span_, float z_, morph::HexDomainShape shape)
{
    this->d = d_;
    this->x_span = x_span_;
    this->z = z_;
    this->domainShape = shape;

    this->init();
}

pair<float, float>
morph::HexGrid::computeCentroid (const list<Hex>& pHexes)
{
    pair<float, float> centroid;
    centroid.first = 0;
    centroid.second = 0;
    for (auto h : pHexes) {
        centroid.first += h.x;
        centroid.second += h.y;
    }
    centroid.first /= pHexes.size();
    centroid.second /= pHexes.size();
    return centroid;
}

void
morph::HexGrid::setBoundary (const list<Hex>& pHexes)
{
    this->boundaryCentroid = this->computeCentroid (pHexes);

    list<Hex>::iterator bpoint = this->hexen.begin();
    list<Hex>::iterator bpi = this->hexen.begin();
    while (bpi != this->hexen.end()) {
        list<Hex>::const_iterator ppi = pHexes.begin();
        while (ppi != pHexes.end()) {
            // NB: The assumption right now is that the pHexes are
            // from the same dimension hex grid as this->hexen.
            if (bpi->ri == ppi->ri && bpi->gi == ppi->gi) {
                // Set h as boundary hex.
                bpi->boundaryHex = true;
                bpoint = bpi;
                break;
            }
            ++ppi;
        }
        ++bpi;
    }

    // Check that the boundary is contiguous.
    set<unsigned int> seen;
    list<Hex>::iterator hi = bpoint;
    if (this->boundaryContiguous (bpoint, hi, seen) == false) {
        stringstream ee;
        ee << "The boundary is not a contiguous sequence of hexes.";
        throw runtime_error (ee.str());
    }

    if (this->domainShape == morph::HexDomainShape::Boundary) {
        // Boundary IS contiguous, discard hexes outside the boundary.
        this->discardOutsideBoundary();
    } else {
        throw runtime_error ("For now, setBoundary (const list<Hex>& pHexes) doesn't know what to do if domain shape is not HexDomainShape::Boundary.");
    }
}

#ifdef UNTESTED_UNUSED
void
morph::HexGrid::offsetCentroid (void)
{
    for (auto h : this->hexen) {
        cout << " * : " << h.x << "," << h.y << endl;
    }
    for (auto h : this->hexen) {
        h.subtractLocation (this->boundaryCentroid);
        cout << "***: " << h.x << "," << h.y << endl;
    }
    this->boundaryCentroid = make_pair (0.0, 0.0);
}
#endif

void
morph::HexGrid::allocateSubPgrams (void)
{
    // Start at centroid. Build out until a central pgram touches the
    // outer edge.
    list<Hex>::iterator centroidHex = this->findHexNearest (this->boundaryCentroid);
    // Iterators to corner hexes:
    list<Hex>::iterator cornerNW = centroidHex->nnw;
    // I disregard the very tip hex of the NE point of the
    // parallelogram (the "pointy" corner - I've decided that the
    // pgram will slant (with the top edge) to the right). So, there
    // are two hexes making up the NE corner of the sub-parallelogram;
    // 'north-north-east' and 'east-north-east'.
    list<Hex>::iterator cornerNNE = centroidHex->nne;
    list<Hex>::iterator cornerENE = centroidHex->ne;

    list<Hex>::iterator cornerSSW = centroidHex->nsw;
    list<Hex>::iterator cornerWSW = centroidHex->nw;

    list<Hex>::iterator cornerSE = centroidHex->nse;

    // Sharp corners
    bool foundNE = false;
    bool foundSW = false;
    // Blunt corners
    bool foundNW = false;
    bool foundSE = false;

    while (foundNE == false || foundSW == false
           || foundNW == false || foundSE == false) {
        DBG ("Corners:"
             << " NNE: (" << cornerNNE->ri << "," << cornerNNE->gi << ")"
             << " ENE: (" << cornerENE->ri << "," << cornerENE->gi << ")"
             << " NW:  (" << cornerNW->ri << "," << cornerNW->gi << ")"
             << " WSW: (" << cornerWSW->ri << "," << cornerWSW->gi << ")"
             << " SSW: (" << cornerSSW->ri << "," << cornerSSW->gi << ")"
             << " SE:  (" << cornerSE->ri << "," << cornerSE->gi << ")"
            );
        DBG ("found: NE: " << foundNE << " SW: " << foundSW << " NW: " << foundNW << " SE: " << foundSE);

        if (cornerNNE->boundaryHex == true
            || cornerENE->boundaryHex == true) {
            DBG ("foundNE due to boundary hex");
            foundNE = true;
        }
        if (cornerNW->boundaryHex == true) {
            DBG ("foundNW due to boundary hex");
            foundNW = true;
        }
        if (cornerSE->boundaryHex == true) {
            DBG ("foundSE due to boundary hex");
            foundSE = true;
        }
        if (cornerSSW->boundaryHex == true
            || cornerWSW->boundaryHex == true) {
            DBG ("foundSW due to boundary hex");
            foundSW = true;
        }

        // Try to move corners out...
        if (foundNE == false || foundNW == false || foundSE == false) {
            DBG ("foundNE or foundNW or foundSE is false");
            // Move NE corner out if possible. This is possible
            // if by moving it out, both the NE corner still exists
            // AND the NW and SE corners ( which much also move in one
            // direction) exist.
            if (cornerNNE->has_ne == true
                && cornerNNE->ne->has_nne == true
                && cornerNNE->ne->has_ne == true
                && cornerNW->has_nne == true
                && cornerSE->has_ne == true) {

                cornerNNE = cornerNNE->ne->nne;
                cornerENE = cornerNNE->ne->ne;
                cornerNW = cornerNW->nne;
                cornerSE = cornerSE->ne;

            } else {
                if (cornerNNE->has_ne == false
                    || (cornerNNE->has_ne == true
                        &&
                        (cornerNNE->ne->has_nne == false || cornerNNE->ne->has_ne == false)
                        )) {
                    DBG ("No neighbour, foundNE is true");
                    foundNE = true;
                    // Nothing more to do but test if we've foundNW and foundSE too...
                    if (cornerNW->has_nw == false) {
                        DBG ("No neighbour west of cornerNW, foundNW is true");
                        foundNW = true;
                    } else {
                        DBG ("Have left edge neighbour to W");
                    }
                    if (cornerSE->has_nsw == false) {
                        DBG ("No neighbour southwest of cornerSE, foundSE is true");
                        foundSE = true;
                    } else {
                        DBG ("Have bottom edge neighbour to SW");
                    }
                } else {
                    if (cornerNW->has_nne == false) {
                        DBG ("No neighbour northest of NW, foundNW is true");
                        foundNW = true;
                        // Can try to move east with SE corner:
                        if (cornerSE->has_ne == true
                            && cornerENE->has_ne == true
                            && cornerNNE->has_ne == true) {
                            DBG ("Extending right edge E");
                            cornerSE = cornerSE->ne;
                            cornerNNE = cornerNNE->ne;
                            cornerENE = cornerNNE->ne;
                        } else {
                            DBG ("Not extending right edge E");
                        }
                    } else {
                        DBG ("cornerNW has neighbour NE, not setting foundNW...");
                    }
                    if (cornerSE->has_ne == false) {
                        DBG ("No neighbour east of SE, foundSE is true");
                        foundSE = true;
                        // Can try to move northeast with NW corner:
                        if (cornerNW->has_nne == true
                            && cornerENE->has_nne == true
                            && cornerNNE->has_nne == true) {
                            DBG ("Extending top edge NE");
                            cornerNW = cornerNW->nne;
                            cornerNNE = cornerNNE->nne;
                            cornerENE = cornerENE->nne;
                        } else {
                            DBG ("Not extending top edge NE");
                        }
                    } else {
                        DBG ("cornerSE has neighbour E, not setting foundSE...");
                    }
                }
            }
        }

        if (foundSW == false || foundNW == false || foundSE == false) {

            DBG ("foundSW or foundNW or foundSE is false");

            if (cornerSSW->has_nw == true
                && cornerSSW->nw->has_nw == true
                && cornerSSW->nw->has_nsw == true
                && cornerNW->has_nw == true
                && cornerSE->has_nsw == true) {

                DBG ("Move 3 SW corners...");
                cornerSSW = cornerSSW->nw->nsw;
                cornerWSW = cornerSSW->nw->nw;
                cornerNW = cornerNW->nw;
                cornerSE = cornerSE->nsw;

            } else {

                DBG ("Not moving 3 SW corneres...");
                if (cornerSSW->has_nw == false
                    || (cornerSSW->has_nw == true
                        &&
                        (cornerSSW->nw->has_nw == false || cornerSSW->nw->has_nsw == false)
                        )) {
                    DBG ("No neighbour, foundSW is true");
                    foundSW = true;
                    // Nothing more to do but test cornerNW and cornerSE
                    if (cornerNW->has_nne == false) {
                        foundNW = true;
                    }
                    if (cornerSE->has_ne == false) {
                        foundSE = true;
                    }
                    if (foundNE == true) {
                        DBG ("foundSW AND foundNE true, so can't exapand in any dirn. Set foundNW and foundSE true");
                        foundNW = true;
                        foundSE = true;
                    }
                } else {
                    DBG ("foundSW not set true");
                    // Not found the absolute southwest tip yet.
                    if (cornerNW->has_nw == false) {
                        DBG ("Set foundNW true as cornerNW has_nw");
                        foundNW = true;
                        // Can try to move bottom edge southwest
                        if (cornerSE->has_nsw == true
                            && cornerSSW->has_nsw == true
                            && cornerWSW->has_nsw == true) {
                            DBG ("Extending bottom edge SW");
                            cornerSE = cornerSE->nsw;
                            cornerSSW = cornerSSW->nsw;
                            cornerWSW = cornerWSW->nsw;
                        }
                    } else {
                        DBG ("foundNW not set true");
                    }
                    if (cornerSE->has_nsw == false) {
                        DBG ("Setting foundSE true as cornerSE->has_nsw == false");
                        foundSE = true;
                        // Can try to move left edge west
                        if (cornerNW->has_nw == true
                            && cornerSSW->has_nw == true
                            && cornerWSW->has_nw == true) {
                            DBG ("Extending left edge W");
                            cornerNW = cornerSE->nw;
                            cornerSSW = cornerSSW->nw;
                            cornerWSW = cornerWSW->nw;
                        }
                    } else {
                        DBG ("foundSE not set true");
                    }
                }
            }
        }
    }

    // Now we have the corners of the sub-parallelogram we can define
    // it. Something similar to markHexesInsideParalelogram, but not
    // quite, because simultaneously need to copy to sp_ vectors.  The
    // bottom left hex will be cornerSSW
    int subpIdx = 0;

    // Only doing a single sub pgram right now, just resize each
    // vectorvector to one.
    this->sp_x.resize(1);
    this->sp_y.resize(1);
    this->sp_ri.resize(1);
    this->sp_gi.resize(1);
    this->sp_bi.resize(1);
    this->sp_distToBoundary.resize(1);
    this->sp_ne.resize(1);
    this->sp_nne.resize(1);
    this->sp_nnw.resize(1);
    this->sp_nw.resize(1);
    this->sp_nsw.resize(1);
    this->sp_nse.resize(1);
    this->sp_v_ne.resize(1);
    this->sp_v_nne.resize(1);
    this->sp_v_nnw.resize(1);
    this->sp_v_nw.resize(1);
    this->sp_v_nsw.resize(1);
    this->sp_v_nse.resize(1);
    this->sp_flags.resize(1);
    this->sp_rowlens.resize(1);
    this->sp_numrows.resize(1);
    this->sp_veclen.resize(1);
    this->sp_numvecs = 1;

    unsigned int rl = cornerSE->ri - (cornerSSW->ri - 1);
    list<Hex>::iterator rowstart = cornerSSW;
    list<Hex>::iterator h = rowstart;
    int bot_gi = cornerSSW->gi;
    int top_gi = cornerNW->gi;
    DBG("bot/top gi: " << bot_gi << ", " << top_gi);

    // Note rowlens are set up so that each row is the same. That
    // means that in data-vectors, the very first element is a dummy
    // element, as is the last. The actual sub-parallelogram has those
    // Hexes omitted.
    this->sp_rowlens[0] = cornerSE->ri - (cornerSSW->ri - 1);
    this->sp_numrows[0] = top_gi - bot_gi + 1;
    this->sp_veclen[0] = this->sp_rowlens[0] * this->sp_numrows[0];
    // The two hexes at the ends of the pointiest corners of the
    // parallelogram aren't included, so subtract 2 to get the correct
    // veclen:
    this->sp_veclen[0] -= 2;

    for (int v = bot_gi; v<=top_gi; ++v) {
        unsigned int rl_ = rl;
        if (v==bot_gi || v==top_gi) {
            --rl_;
            if (rowstart->has_nnw) {
                rowstart = rowstart->nnw;
            }
        } else {
            if (rowstart->has_nne) {
                rowstart = rowstart->nne;
            }
        }
        for (unsigned int j = 0; j<rl_; ++j) {
            h->allocatedSubp = subpIdx;
            this->sp_push_back (h);
            h = h->ne;
        }
        h = rowstart;
    }
    DBG ("Done.");
}

//#define DONT_OFFSET_CENTROID_BEFORE_SETTING_BOUNDARY 1
void
morph::HexGrid::setBoundary (const BezCurvePath& p)
{
    this->boundary = p;

    if (!this->boundary.isNull()) {
        DBG ("Applying boundary...");

        // Compute the points on the boundary using half of the hex to
        // hex spacing as the step size.
        vector<BezCoord> bpoints = this->boundary.getPoints (this->d/2.0f, true); // true to invert y axis

        this->boundaryCentroid = BezCurvePath::getCentroid (bpoints);
        DBG ("Boundary centroid: " << boundaryCentroid.first << "," << boundaryCentroid.second);

#ifdef DONT_OFFSET_CENTROID_BEFORE_SETTING_BOUNDARY
        list<Hex>::iterator nearbyBoundaryPoint = this->findHexNearest (this->boundaryCentroid);
        DBG ("Hex near boundary centroid at x,y: " << nearbyBoundaryPoint->x << "," << nearbyBoundaryPoint->y);
        auto bpi = bpoints.begin();
#else // Offset BezCoords of the boundary BezCurvePath by its centroid, to make the centroid 0,0.
        auto bpi = bpoints.begin();
        while (bpi != bpoints.end()) {
            bpi->subtract (this->boundaryCentroid);
            ++bpi;
        }
        this->boundaryCentroid = make_pair (0.0, 0.0);
        list<Hex>::iterator nearbyBoundaryPoint = this->hexen.begin(); // i.e the Hex at 0,0
        bpi = bpoints.begin();
#endif
        while (bpi != bpoints.end()) {
            nearbyBoundaryPoint = this->setBoundary (*bpi++, nearbyBoundaryPoint);
            DBG2 ("Added boundary point " << nearbyBoundaryPoint->ri << "," << nearbyBoundaryPoint->gi);
        }

        // Check that the boundary is contiguous.
        {
            set<unsigned int> seen;
            list<Hex>::iterator hi = nearbyBoundaryPoint;
            if (this->boundaryContiguous (nearbyBoundaryPoint, hi, seen) == false) {
                stringstream ee;
                ee << "The boundary which was constructed from "
                   << p.name << " is not a contiguous sequence of hexes.";
                throw runtime_error (ee.str());
            }
        }

        if (this->domainShape == morph::HexDomainShape::Boundary
            || this->domainShape == morph::HexDomainShape::SubParallelograms) {

            this->discardOutsideBoundary();

            if (this->domainShape == morph::HexDomainShape::SubParallelograms) {
                // Do something. Populate sp_vectors Are these now
                // vectors of vectors? THEN populate d_ vectors with
                // the remaining hexes.
                this->allocateSubPgrams();

                // Now populate the d_ vectors
                list<Hex>::iterator hi = this->hexen.begin();
                while (hi != this->hexen.end()) {
                    if (hi->allocatedSubp == -1) {
                        this->d_push_back (hi);
                    }
                    hi++;
                }

                this->populate_sp_d_neighbours();

            } else {
                // Now populate the d_ vectors
                list<Hex>::iterator hi = this->hexen.begin();
                while (hi != this->hexen.end()) {
                    this->d_push_back (hi);
                    hi++;
                }
                this->populate_d_neighbours();
            }

        } else {
            // Given that the boundary IS contiguous, can now set a
            // domain of hexes (rectangular, parallelogram or
            // hexagonal region, such that computations can be
            // efficient) and discard hexes outside the domain.
            // setDomain() will define a regular domain, then discard
            // those hexes outside the regular domain and populate all
            // the d_ vectors.
            this->setDomain();
        }
    }
}

list<Hex>::iterator
morph::HexGrid::setBoundary (const BezCoord& point, list<Hex>::iterator startFrom)
{
    // Searching from "startFrom", search out, via neighbours until
    // the hex closest to the boundary point is located. How to know
    // if it's closest? When all neighbours are further from the
    // currently closest point?

    bool neighbourNearer = true;

    list<Hex>::iterator h = startFrom;
    float d = h->distanceFrom (point);
    float d_ = 0.0f;

    while (neighbourNearer == true) {

        neighbourNearer = false;
        if (h->has_ne && (d_ = h->ne->distanceFrom (point)) < d) {
            d = d_;
            h = h->ne;
            neighbourNearer = true;

        } else if (h->has_nne && (d_ = h->nne->distanceFrom (point)) < d) {
            d = d_;
            h = h->nne;
            neighbourNearer = true;

        } else if (h->has_nnw && (d_ = h->nnw->distanceFrom (point)) < d) {
            d = d_;
            h = h->nnw;
            neighbourNearer = true;

        } else if (h->has_nw && (d_ = h->nw->distanceFrom (point)) < d) {
            d = d_;
            h = h->nw;
            neighbourNearer = true;

        } else if (h->has_nsw && (d_ = h->nsw->distanceFrom (point)) < d) {
            d = d_;
            h = h->nsw;
            neighbourNearer = true;

        } else if (h->has_nse && (d_ = h->nse->distanceFrom (point)) < d) {
            d = d_;
            h = h->nse;
            neighbourNearer = true;
        }
    }

    DBG2 ("Nearest hex to point (" << point.x() << "," << point.y() << ") is at (" << h->ri << "," << h->gi << ")");

    // Mark it for being on the boundary
    h->boundaryHex = true;

    return h;
}

bool
morph::HexGrid::findBoundaryHex (list<Hex>::const_iterator& hi) const
{
    DBG ("Testing Hex ri,gi = " << hi->ri << "," << hi->gi << " x,y = " << hi->x << "," << hi->y);
    if (hi->boundaryHex == true) {
        // No need to change the Hex iterator
        return true;
    }

    if (hi->has_ne) {
        list<Hex>::const_iterator ci(hi->ne);
        if (this->findBoundaryHex (ci) == true) {
            hi = ci;
            return true;
        }
    }
    if (hi->has_nne) {
        list<Hex>::const_iterator ci(hi->nne);
        if (this->findBoundaryHex (ci) == true) {
            hi = ci;
            return true;
        }
    }
    if (hi->has_nnw) {
        list<Hex>::const_iterator ci(hi->nnw);
        if (this->findBoundaryHex (ci) == true) {
            hi = ci;
            return true;
        }
    }
    if (hi->has_nw) {
        list<Hex>::const_iterator ci(hi->nw);
        if (this->findBoundaryHex (ci) == true) {
            hi = ci;
            return true;
        }
    }
    if (hi->has_nsw) {
        list<Hex>::const_iterator ci(hi->nsw);
        if (this->findBoundaryHex (ci) == true) {
            hi = ci;
            return true;
        }
    }
    if (hi->has_nse) {
        list<Hex>::const_iterator ci(hi->nse);
        if (this->findBoundaryHex (ci) == true) {
            hi = ci;
            return true;
        }
    }

    return false;
}

bool
morph::HexGrid::boundaryContiguous (void) const
{
    list<Hex>::const_iterator bhi = this->hexen.begin();
    if (this->findBoundaryHex (bhi) == false) {
        // Found no boundary hex
        return false;
    }
    set<unsigned int> seen;
    list<Hex>::const_iterator hi = bhi;
    return this->boundaryContiguous (bhi, hi, seen);
}

bool
morph::HexGrid::boundaryContiguous (list<Hex>::const_iterator bhi, list<Hex>::const_iterator hi, set<unsigned int>& seen) const
{
    DBG2 ("Called for hi=" << hi->vi);
    bool rtn = false;
    list<Hex>::const_iterator hi_next;

    DBG2 ("Inserting " << hi->vi << " into seen which is Hex ("<< hi->ri << "," << hi->gi<<")");
    seen.insert (hi->vi);

    DBG2 (hi->output());

    if (rtn == false && hi->has_ne && hi->ne->boundaryHex == true && seen.find(hi->ne->vi) == seen.end()) {
        hi_next = hi->ne;
        rtn = (this->boundaryContiguous (bhi, hi_next, seen));
    }
    if (rtn == false && hi->has_nne && hi->nne->boundaryHex == true && seen.find(hi->nne->vi) == seen.end()) {
        hi_next = hi->nne;
        rtn = this->boundaryContiguous (bhi, hi_next, seen);
    }
    if (rtn == false && hi->has_nnw && hi->nnw->boundaryHex == true && seen.find(hi->nnw->vi) == seen.end()) {
        hi_next = hi->nnw;
        rtn =  (this->boundaryContiguous (bhi, hi_next, seen));
    }
    if (rtn == false && hi->has_nw && hi->nw->boundaryHex == true && seen.find(hi->nw->vi) == seen.end()) {
        hi_next = hi->nw;
        rtn =  (this->boundaryContiguous (bhi, hi_next, seen));
    }
    if (rtn == false && hi->has_nsw && hi->nsw->boundaryHex == true && seen.find(hi->nsw->vi) == seen.end()) {
        hi_next = hi->nsw;
        rtn =  (this->boundaryContiguous (bhi, hi_next, seen));
    }
    if (rtn == false && hi->has_nse && hi->nse->boundaryHex == true && seen.find(hi->nse->vi) == seen.end()) {
        hi_next = hi->nse;
        rtn =  (this->boundaryContiguous (bhi, hi_next, seen));
    }

    if (rtn == false) {
        // Checked all neighbours
        if (hi == bhi) {
            DBG2 ("Back at start, nowhere left to go! return true.");
            rtn = true;
        } else {
            DBG2 ("Back at hi=(" << hi->ri << "," << hi->gi << "), while bhi=(" <<  bhi->ri << "," << bhi->gi << "), return false");
            rtn = false;
        }
    }

    DBG2 ("Boundary " << (rtn ? "IS" : "isn't") << " contiguous so far...");

    return rtn;
}

void
morph::HexGrid::markHexesInside (list<Hex>::iterator hi)
{
    if (hi->boundaryHex == true) {
        hi->insideBoundary = true;
        return;

    } else if (hi->insideBoundary == true) {
        return;

    } else {

        hi->insideBoundary = true;

        if (hi->has_ne) {
            this->markHexesInside (hi->ne);
        }
        if (hi->has_nne) {
            this->markHexesInside (hi->nne);
        }
        if (hi->has_nnw) {
            this->markHexesInside (hi->nnw);
        }
        if (hi->has_nw) {
            this->markHexesInside (hi->nw);
        }
        if (hi->has_nsw) {
            this->markHexesInside (hi->nsw);
        }
        if (hi->has_nse) {
            this->markHexesInside (hi->nse);
        }
    }
}

void
morph::HexGrid::markHexesInsideRectangularDomain (const array<int, 6>& extnts)
{
    // Check ri,gi,bi and reduce to equivalent ri,gi,bi=0.
    // Use gi to determine whether outside top/bottom region
    // Add gi contribution to ri to determine whether outside left/right region

    // Is the bottom row's gi even or odd?  extnts[2] is gi for the
    // bottom row. If it's even, then we add 0.5 to all rows with even
    // gi. If it's odd then we add 0.5 to all rows with ODD gi.
    float even_addn = 0.5f;
    float odd_addn = 0.0f;
    float addleft = 0;
    if (extnts[2]%2 == 0) {
        DBG ("bottom row has EVEN gi (" << extnts[2] << ")");
        even_addn = 0.0f;
        odd_addn = 0.5f;
    } else {
        DBG ("bottom row has ODD gi (" << extnts[2] << ")");
        addleft += 0.5f;
    }

    if (abs(extnts[2]%2) == abs(extnts[4]%2)) {
        // Left most hex is on a parity-matching line to bottom line,
        // no need to add left.
        DBG("Left most hex on line matching parity of bottom line")
    } else {
        // Need to add left.
        DBG("Left most hex NOT on line matching parity of bottom line add left to BL hex");
        if (extnts[2]%2 == 0) {
            addleft += 1.0f;
            // For some reason, only in this case do we addleft (and
            // not in the case where BR is ODD and Left most hex NOT
            // matching, which makes addleft = 0.5 + 0.5). I can't
            // work it out.
            DBG ("Before: d_rowlen " << d_rowlen << " d_size " << d_size);
            this->d_rowlen += addleft;
            this->d_size = this->d_rowlen * this->d_numrows;
            DBG ("after: d_rowlen " << d_rowlen << " d_size " << d_size);
        } else {
            addleft += 0.5f;
        }
    }

    DBG ("FINAL addleft is: " << addleft);

    auto hi = this->hexen.begin();
    while (hi != this->hexen.end()) {

        // Here, hz is "horizontal index", made up of the ri index,
        // half the gi index.
        //
        // plus a row-varying addition of a half
        // (the row of hexes above is shifted right by 0.5 a hex
        // width).
        float hz = hi->ri + 0.5*(hi->gi); /*+ (hi->gi%2 ? odd_addn : even_addn)*/;
        float parityhalf = (hi->gi%2 ? odd_addn : even_addn);

        if (hz < (extnts[0] - addleft + parityhalf)) {
            // outside
            DBG2 ("Outside. gi:"<<hi->gi<<". Horz idx: " << hz << " < extnts[0]-addleft+parityhalf: " << extnts[0] <<"-"<< addleft <<"+"<< parityhalf);
        } else if (hz > (extnts[1] + parityhalf)) {
            // outside
            DBG2 ("Outside. gi:"<<hi->gi<<". Horz idx: " << hz << " > extnts[1]+parityhalf: " << extnts[0] <<"+"<< parityhalf);
        } else if (hi->gi < extnts[2]) {
            // outside
            DBG2 ("Outside. Vert idx: " << hi->gi << " < extnts[2]: " << extnts[2]);
        } else if (hi->gi > extnts[3]) {
            // outside
            DBG2 ("Outside. Vert idx: " << hi->gi << " > extnts[3]: " << extnts[3]);
        } else {
            // inside
            DBG2 ("INSIDE. Horz,vert index: " << hz << "," << hi->gi);
            hi->insideDomain = true;
        }
        ++hi;
    }
}

void
morph::HexGrid::markHexesInsideParallelogramDomain (const array<int, 6>& extnts)
{
    // Check ri,gi,bi and reduce to equivalent ri,gi,bi=0.
    // Use gi to determine whether outside top/bottom region
    // Add gi contribution to ri to determine whether outside left/right region
    auto hi = this->hexen.begin();
    while (hi != this->hexen.end()) {
        if (hi->ri < extnts[0]
            || hi->ri > extnts[1]
            || hi->gi < extnts[2]
            || hi->gi > extnts[3]) {
            // outside
        } else {
            // inside
            hi->insideDomain = true;
        }
        ++hi;
    }
}

void
morph::HexGrid::markAllHexesInsideDomain (void)
{
    list<Hex>::iterator hi = this->hexen.begin();
    while (hi != this->hexen.end()) {
        hi->insideDomain = true;
        hi++;
    }
}

void
morph::HexGrid::computeDistanceToBoundary (void)
{
    list<Hex>::iterator h = this->hexen.begin();
    while (h != this->hexen.end()) {
        if (h->boundaryHex == true) {
            h->distToBoundary = 0.0f;
        } else {
            if (h->insideBoundary == false) {
                // Set to a dummy, negative value
                h->distToBoundary = -100.0;
            } else {
                // Not a boundary hex, but inside boundary
                list<Hex>::iterator bh = this->hexen.begin();
                while (bh != this->hexen.end()) {
                    if (bh->boundaryHex == true) {
                        float delta = h->distanceFrom (*bh);
                        if (delta < h->distToBoundary || h->distToBoundary < 0.0f) {
                            h->distToBoundary = delta;
                        }
                    }
                    ++bh;
                }
            }
        }
        DBG2 ("Hex: " << h->vi <<"  d to bndry: " << h->distToBoundary
              << " on bndry? " << (h->boundaryHex?"Y":"N"));
        ++h;
    }
}

array<int, 6>
morph::HexGrid::findBoundaryExtents (void)
{
    // Return object contains {ri-left, ri-right, gi-bottom, gi-top, gi at ri-left, gi at ri-right}
    // i.e. {xmin, xmax, ymin, ymax, gi at xmin, gi at xmax}
    array<int, 6> rtn = {{0,0,0,0,0,0}};

    // Find the furthest left and right hexes and the further up and down hexes.
    array<float, 4> limits = {{0,0,0,0}};
    bool first = true;
    for (auto h : this->hexen) {
        if (h.boundaryHex == true) {
            if (first) {
                limits = {{h.x, h.x, h.y, h.y}};
                first = false;
            }
            if (h.x < limits[0]) {
                limits[0] = h.x;
                rtn[4] = h.gi;
            }
            if (h.x > limits[1]) {
                limits[1] = h.x;
                rtn[5] = h.gi;
            }
            if (h.y < limits[2]) {
                limits[2] = h.y;
            }
            if (h.y > limits[3]) {
                limits[3] = h.y;
            }
        }
    }

    // Now compute the ri and gi values that these xmax/xmin/ymax/ymin
    // correspond to. THIS, if nothing else, should auto-vectorise!
    // d_ri is the distance moved in ri direction per x, d_gi is distance
    float d_ri = this->hexen.front().getD();
    float d_gi = this->hexen.front().getV();
    rtn[0] = (int)(limits[0] / d_ri);
    rtn[1] = (int)(limits[1] / d_ri);
    rtn[2] = (int)(limits[2] / d_gi);
    rtn[3] = (int)(limits[3] / d_gi);

    DBG ("ll,lr,lb,lt:     {" << limits[0] << ","  << limits[1] << ","  << limits[2] << ","  << limits[3] << "}");
    DBG ("d_ri: " << d_ri << ", d_gi: " << d_gi);
    DBG ("ril,rir,gib,git: {" << rtn[0] << ","  << rtn[1] << ","  << rtn[2] << ","  << rtn[3] << "}");

    // Add 'growth buffer'
    rtn[0] -= this->d_growthbuffer_horz;
    rtn[1] += this->d_growthbuffer_horz;
    rtn[2] -= this->d_growthbuffer_vert;
    rtn[3] += this->d_growthbuffer_vert;

    return rtn;
}

void
morph::HexGrid::d_clear (void)
{
    this->d_x.clear();
    this->d_y.clear();
    this->d_ri.clear();
    this->d_gi.clear();
    this->d_bi.clear();
    this->d_flags.clear();
}

void
morph::HexGrid::sp_push_back (list<Hex>::iterator hi)
{
    int subpIdx = hi->allocatedSubp;

    this->sp_x[subpIdx].push_back (hi->x);
    this->sp_y[subpIdx].push_back (hi->y);
    this->sp_ri[subpIdx].push_back (hi->ri);
    this->sp_gi[subpIdx].push_back (hi->gi);
    this->sp_bi[subpIdx].push_back (hi->bi);

    this->sp_flags[subpIdx].push_back (hi->getFlags());
    this->sp_distToBoundary[subpIdx].push_back (hi->distToBoundary);

    // record in the Hex the iterator in the sp_ vectors so that d_nne
    // and friends can be set up later.
    hi->di = sp_x[subpIdx].size()-1;
}
void
morph::HexGrid::d_push_back (list<Hex>::iterator hi)
{
    d_x.push_back (hi->x);
    d_y.push_back (hi->y);
    d_ri.push_back (hi->ri);
    d_gi.push_back (hi->gi);
    d_bi.push_back (hi->bi);
    d_flags.push_back (hi->getFlags());
    d_distToBoundary.push_back (hi->distToBoundary);

    // record in the Hex the iterator in the d_ vectors so that d_nne
    // and friends can be set up later.
    hi->di = d_x.size()-1;
    // by default hi->allocatedSubp should be -1;
    if (hi->allocatedSubp != -1) {
        throw runtime_error ("by default hi->allocatedSubp should be -1 but it isn't");
    }
}

void
morph::HexGrid::populate_d_neighbours (void)
{
    // Resize d_nne and friends
    this->d_nne.resize (this->d_x.size(), 0);
    this->d_ne.resize (this->d_x.size(), 0);
    this->d_nnw.resize (this->d_x.size(), 0);
    this->d_nw.resize (this->d_x.size(), 0);
    this->d_nsw.resize (this->d_x.size(), 0);
    this->d_nse.resize (this->d_x.size(), 0);

    this->d_v_nne.resize (this->d_x.size(), 0);
    this->d_v_ne.resize (this->d_x.size(), 0);
    this->d_v_nnw.resize (this->d_x.size(), 0);
    this->d_v_nw.resize (this->d_x.size(), 0);
    this->d_v_nsw.resize (this->d_x.size(), 0);
    this->d_v_nse.resize (this->d_x.size(), 0);

    list<Hex>::iterator hi = this->hexen.begin();
    while (hi != this->hexen.end()) {

        // All indices in d_ne, d_nne, etc index into the d_ vectors,
        // so d_v_ne, etc are always equal to -1.
        this->d_v_ne[hi->di] = -1;
        this->d_v_nne[hi->di] = -1;
        this->d_v_nnw[hi->di] = -1;
        this->d_v_nw[hi->di] = -1;
        this->d_v_nsw[hi->di] = -1;
        this->d_v_nse[hi->di] = -1;

        if (hi->has_ne == true) {
            this->d_ne[hi->di] = hi->ne->di;
        } else {
            this->d_ne[hi->di] = -1;
        }
        if (hi->has_nne == true) {
            this->d_nne[hi->di] = hi->nne->di;
        } else {
            this->d_nne[hi->di] = -1;
        }
        if (hi->has_nnw == true) {
            this->d_nnw[hi->di] = hi->nnw->di;
        } else {
            this->d_nnw[hi->di] = -1;
        }
        if (hi->has_nw == true) {
            this->d_nw[hi->di] = hi->nw->di;
        } else {
            this->d_nw[hi->di] = -1;
        }
        if (hi->has_nsw == true) {
            this->d_nsw[hi->di] = hi->nsw->di;
        } else {
            this->d_nsw[hi->di] = -1;
        }
        if (hi->has_nse == true) {
            this->d_nse[hi->di] = hi->nse->di;
        } else {
            this->d_nse[hi->di] = -1;
        }
#ifdef DEBUG
        if (hi->di == 1075 || hi->di == 1076) {
            DBG("Hex in d_ vector position " << hi->di << " has NNE: " << this->d_nne[hi->di]
                << ", NNW: " << this->d_nnw[hi->di]
                << ", NSW: " << this->d_nsw[hi->di]
                << ", NSE: " << this->d_nse[hi->di]);
        }
#endif
        ++hi;
    }
}

void
morph::HexGrid::populate_sp_d_neighbours (void)
{
    // Resize d_nne and friends
    this->d_nne.resize (this->d_x.size(), 0);
    this->d_ne.resize (this->d_x.size(), 0);
    this->d_nnw.resize (this->d_x.size(), 0);
    this->d_nw.resize (this->d_x.size(), 0);
    this->d_nsw.resize (this->d_x.size(), 0);
    this->d_nse.resize (this->d_x.size(), 0);

    this->d_v_nne.resize (this->d_x.size(), -1);
    this->d_v_ne.resize (this->d_x.size(), -1);
    this->d_v_nnw.resize (this->d_x.size(), -1);
    this->d_v_nw.resize (this->d_x.size(), -1);
    this->d_v_nsw.resize (this->d_x.size(), -1);
    this->d_v_nse.resize (this->d_x.size(), -1);

    // Resize sp_nne and friends
    for (unsigned int vi = 0; vi < this->sp_x.size(); ++vi) {
        this->sp_nne[vi].resize (this->sp_x[vi].size());
        this->sp_ne[vi].resize (this->sp_x[vi].size());
        this->sp_nnw[vi].resize (this->sp_x[vi].size());
        this->sp_nw[vi].resize (this->sp_x[vi].size());
        this->sp_nsw[vi].resize (this->sp_x[vi].size());
        this->sp_nse[vi].resize (this->sp_x[vi].size());

        this->sp_v_nne[vi].resize (this->sp_x[vi].size());
        this->sp_v_ne[vi].resize (this->sp_x[vi].size());
        this->sp_v_nnw[vi].resize (this->sp_x[vi].size());
        this->sp_v_nw[vi].resize (this->sp_x[vi].size());
        this->sp_v_nsw[vi].resize (this->sp_x[vi].size());
        this->sp_v_nse[vi].resize (this->sp_x[vi].size());
    }

    list<Hex>::iterator hi = this->hexen.begin();
    while (hi != this->hexen.end()) {

        if (hi->has_ne == true) {
            if (hi->allocatedSubp == -1) {
                this->d_ne[hi->di] = hi->ne->di;
                this->d_v_ne[hi->di] = hi->ne->allocatedSubp;
            } else {
                this->sp_ne[hi->allocatedSubp][hi->di] = hi->ne->di;
                this->sp_v_ne[hi->allocatedSubp][hi->di] = hi->ne->allocatedSubp;
            }
        } else {
            if (hi->allocatedSubp == -1) {
                this->d_ne[hi->di] = -1;
                this->d_v_ne[hi->di] = -1;
            } else {
                this->sp_ne[hi->allocatedSubp][hi->di] = -1;
                this->sp_v_ne[hi->allocatedSubp][hi->di] = -1;
            }
        }

        if (hi->has_nne == true) {
            if (hi->allocatedSubp == -1) {
                this->d_nne[hi->di] = hi->nne->di;
                this->d_v_nne[hi->di] = hi->nne->allocatedSubp;
            } else {
                this->sp_nne[hi->allocatedSubp][hi->di] = hi->nne->di;
                this->sp_v_nne[hi->allocatedSubp][hi->di] = hi->nne->allocatedSubp;
            }
        } else {
            if (hi->allocatedSubp == -1) {
                this->d_nne[hi->di] = -1;
                this->d_v_nne[hi->di] = -1;
            } else {
                this->sp_nne[hi->allocatedSubp][hi->di] = -1;
                this->sp_v_nne[hi->allocatedSubp][hi->di] = -1;
            }
        }

        if (hi->has_nnw == true) {
            if (hi->allocatedSubp == -1) {
                this->d_nnw[hi->di] = hi->nnw->di;
                this->d_v_nnw[hi->di] = hi->nnw->allocatedSubp;
            } else {
                this->sp_nnw[hi->allocatedSubp][hi->di] = hi->nnw->di;
                this->sp_v_nnw[hi->allocatedSubp][hi->di] = hi->nnw->allocatedSubp;
            }
        } else {
            if (hi->allocatedSubp == -1) {
                this->d_nnw[hi->di] = -1;
                this->d_v_nnw[hi->di] = -1;
            } else {
                this->sp_nnw[hi->allocatedSubp][hi->di] = -1;
                this->sp_v_nnw[hi->allocatedSubp][hi->di] = -1;
            }
        }

        if (hi->has_nw == true) {
            if (hi->allocatedSubp == -1) {
                this->d_nw[hi->di] = hi->nw->di;
                this->d_v_nw[hi->di] = hi->nw->allocatedSubp;
            } else {
                this->sp_nw[hi->allocatedSubp][hi->di] = hi->nw->di;
                this->sp_v_nw[hi->allocatedSubp][hi->di] = hi->nw->allocatedSubp;
            }
        } else {
            if (hi->allocatedSubp == -1) {
                this->d_nw[hi->di] = -1;
                this->d_v_nw[hi->di] = -1;
            } else {
                this->sp_nw[hi->allocatedSubp][hi->di] = -1;
                this->sp_v_nw[hi->allocatedSubp][hi->di] = -1;
            }
        }

        if (hi->has_nsw == true) {
            if (hi->allocatedSubp == -1) {
                this->d_nsw[hi->di] = hi->nsw->di;
                this->d_v_nsw[hi->di] = hi->nsw->allocatedSubp;
            } else {
                this->sp_nsw[hi->allocatedSubp][hi->di] = hi->nsw->di;
                this->sp_v_nsw[hi->allocatedSubp][hi->di] = hi->nsw->allocatedSubp;
            }
        } else {
            if (hi->allocatedSubp == -1) {
                this->d_nsw[hi->di] = -1;
                this->d_v_nsw[hi->di] = -1;
            } else {
                this->sp_nsw[hi->allocatedSubp][hi->di] = -1;
                this->sp_v_nsw[hi->allocatedSubp][hi->di] = -1;
            }
        }

        if (hi->has_nse == true) {
            if (hi->allocatedSubp == -1) {
                this->d_nse[hi->di] = hi->nse->di;
                this->d_v_nse[hi->di] = hi->nse->allocatedSubp;
            } else {
                this->sp_nse[hi->allocatedSubp][hi->di] = hi->nse->di;
                this->sp_v_nse[hi->allocatedSubp][hi->di] = hi->nse->allocatedSubp;
            }
        } else {
            if (hi->allocatedSubp == -1) {
                this->d_nse[hi->di] = -1;
                this->d_v_nse[hi->di] = -1;
            } else {
                this->sp_nse[hi->allocatedSubp][hi->di] = -1;
                this->sp_v_nse[hi->allocatedSubp][hi->di] = -1;
            }
        }

        ++hi;
    }
}

void
morph::HexGrid::setDomain (void)
{
    // 1. Find extent of boundary, both left/right and up/down, with
    // 'buffer region' already added.
    array<int, 6> extnts = this->findBoundaryExtents();

    // 1.5 set rowlen and numrows
    this->d_rowlen = extnts[1]-extnts[0]+1;
    this->d_numrows = extnts[3]-extnts[2]+1;
    this->d_size = this->d_rowlen * this->d_numrows;
    DBG("Initially, d_rowlen=" << d_rowlen << ", d_numrows=" << d_numrows << ", d_size=" << d_size);

    if (this->domainShape == morph::HexDomainShape::Rectangle) {
        // 2. Mark Hexes inside and outside the domain.
        // Mark those hexes inside the boundary
        this->markHexesInsideRectangularDomain (extnts);
    } else if (this->domainShape == morph::HexDomainShape::Parallelogram) {
        this->markHexesInsideParallelogramDomain (extnts);
    } else if (this->domainShape == morph::HexDomainShape::Hexagon) {
        // The original domain was hexagonal, so just mark ALL of them
        // as being in the domain.
        this->markAllHexesInsideDomain();
    } else {
        throw runtime_error ("Unknown HexDomainShape");
    }

    // 3. Discard hexes outside domain
    this->discardOutsideDomain();

    // 3.5 Mark hexes inside boundary
    list<Hex>::iterator centroidHex = this->findHexNearest (this->boundaryCentroid);
    this->markHexesInside (centroidHex);
#ifdef DEBUG
    {
        // Do a little count of them:
        unsigned int numInside = 0;
        unsigned int numOutside = 0;
        for (auto hi : this->hexen) {
            if (hi.insideBoundary == true) {
                ++numInside;
            } else {
                ++numOutside;
            }
        }
        DBG ("Num inside: " << numInside << "; num outside: " << numOutside);
    }
#endif

    // Before populating d_ vectors, also compute the distance to boundary
    this->computeDistanceToBoundary();

    // 4. Populate d_ vectors
    this->populate_d_vectors (extnts);
}

void
morph::HexGrid::populate_d_vectors (const array<int, 6>& extnts)
{
    // First, find the starting hex. For Rectangular and parallelogram
    // domains, that's the bottom left hex.
    list<Hex>::iterator hi = this->hexen.begin();
    // bottom left hex.
    list<Hex>::iterator blh = this->hexen.end();

    if (this->domainShape == morph::HexDomainShape::Rectangle
        || this->domainShape == morph::HexDomainShape::Parallelogram) {

        // Use neighbour relations to go from bottom left to top right.
        // Find hex on bottom row.
        while (hi != this->hexen.end()) {
            if (hi->gi == extnts[2]) {
                // We're on the bottom row
                break;
            }
            ++hi;
        }
        DBG ("hi is on bottom row, posn xy:" << hi->x << "," << hi->y << " or rg:" << hi->ri << "," << hi->gi);
        while (hi->has_nw == true) {
            hi = hi->nw;
        }
        DBG ("hi is at bottom left posn xy:" << hi->x << "," << hi->y << " or rg:" << hi->ri << "," << hi->gi);

        // hi should now be the bottom left hex.
        blh = hi;

        // Sanity check
        if (blh->has_nne == false || blh->has_ne == false || blh->has_nnw == true) {
            stringstream ee;
            ee << "We expect the bottom left hex to have an east and a "
               << "north east neighbour, but no north west neighbour. This has: "
               << (blh->has_nne == true ? "Neighbour NE ":"NO Neighbour NE ")
               << (blh->has_ne == true ? "Neighbour E ":"NO Neighbour E ")
               << (blh->has_nnw == true ? "Neighbour NW ":"NO Neighbour NW ");
            throw runtime_error (ee.str());
        }

    } // else Hexagon or Boundary starts from 0, hi already set to hexen.begin();

    // Clear the d_ vectors.
    this->d_clear();

    // Now raster through the hexes, building the d_ vectors.
    if (this->domainShape == morph::HexDomainShape::Rectangle) {
        bool next_row_ne = true;
        this->d_push_back (hi);
        do {
            hi = hi->ne;

            this->d_push_back (hi);

            DBG2 ("Pushed back flags: " << hi->getFlags() << " for r/g: " << hi->ri << "," << hi->gi);

            if (hi->has_ne == false) {
                if (hi->gi == extnts[3]) {
                    // last (i.e. top) row and no neighbour east, so finished.
                    DBG ("Fin. (top row)");
                    break;
                } else {

                    if (next_row_ne == true) {
                        hi = blh->nne;
                        next_row_ne = false;
                        blh = hi;
                    } else {
                        hi = blh->nnw;
                        next_row_ne = true;
                        blh = hi;
                    }

                    this->d_push_back (hi);
                }
            }
        } while (hi->has_ne == true);

    } else if (this->domainShape == morph::HexDomainShape::Parallelogram) {

        this->d_push_back (hi); // Push back the first one, which is guaranteed to have a NE
        while (hi->has_ne == true) {

            // Step to new hex to the E
            hi = hi->ne;

            if (hi->has_ne == false) {
                // New hex has no NE, so it is on end of row.
                if (hi->gi == extnts[3]) {
                    // on end of top row and no neighbour east, so finished.
                    DBG ("Fin. (top row)");
                    // push back and break
                    this->d_push_back (hi);
                    break;
                } else {
                    // On end of non-top row, so push back...
                    this->d_push_back (hi);
                    // do the 'carriage return'...
                    hi = blh->nne;
                    // And push that back...
                    this->d_push_back (hi);
                    // Update the new 'start of last row' iterator
                    blh = hi;
                }
            } else {
                // New hex does have neighbour east, so just push it back.
                this->d_push_back (hi);
            }
        }

    } else { // Hexagon or Boundary

        while (hi != this->hexen.end()) {
            this->d_push_back (hi);
            hi++;
        }
    }
    DBG ("Size of d_x: " << this->d_x.size() << " and d_size=" << this->d_size);
    this->populate_d_neighbours();
}

void
morph::HexGrid::discardOutsideDomain (void)
{
    // Run through and discard those hexes outside the boundary:
    auto hi = this->hexen.begin();
    while (hi != this->hexen.end()) {
        if (hi->insideDomain == false) {
            // When erasing a Hex, I need to update the neighbours of
            // its neighbours.
            hi->disconnectNeighbours();
            // Having disconnected the neighbours, erase the Hex.
            hi = this->hexen.erase (hi);
        } else {
            ++hi;
        }
    }
    DBG("Number of hexes in this->hexen is now: " << this->hexen.size());

    // The Hex::vi indices need to be re-numbered.
    this->renumberVectorIndices();

    // Finally, do something about the hexagonal grid vertices; set
    // this to true to mark that the iterators to the outermost
    // vertices are no longer valid and shouldn't be used.
    this->gridReduced = true;
}

void
morph::HexGrid::discardOutsideBoundary (void)
{
    // Mark those hexes inside the boundary
    list<Hex>::iterator centroidHex = this->findHexNearest (this->boundaryCentroid);
    this->markHexesInside (centroidHex);

#ifdef DEBUG
    // Do a little count of them:
    unsigned int numInside = 0;
    unsigned int numOutside = 0;
    for (auto hi : this->hexen) {
        if (hi.insideBoundary == true) {
            ++numInside;
        } else {
            ++numOutside;
        }
    }
    DBG("Num inside: " << numInside << "; num outside: " << numOutside);
#endif

    // Run through and discard those hexes outside the boundary:
    auto hi = this->hexen.begin();
    while (hi != this->hexen.end()) {
        if (hi->insideBoundary == false) {
            // Here's the problem I think. When erasing a Hex, I need
            // to update the neighbours of its neighbours.
            hi->disconnectNeighbours();
            // Having disconnected the neighbours, erase the Hex.
            hi = this->hexen.erase (hi);
        } else {
            ++hi;
        }
    }
    DBG("Number of hexes in this->hexen is now: " << this->hexen.size());

    // The Hex::vi indices need to be re-numbered.
    this->renumberVectorIndices();

    // Finally, do something about the hexagonal grid vertices; set
    // this to true to mark that the iterators to the outermost
    // vertices are no longer valid and shouldn't be used.
    this->gridReduced = true;
}

list<Hex>::iterator
morph::HexGrid::findHexNearest (const pair<float, float>& pos)
{
    list<Hex>::iterator nearest = this->hexen.end();
    list<Hex>::iterator hi = this->hexen.begin();
    float dist = FLT_MAX;
    while (hi != this->hexen.end()) {
        float dx = pos.first - hi->x;
        float dy = pos.second - hi->y;
        float dl = sqrt (dx*dx + dy*dy);
        if (dl < dist) {
            dist = dl;
            nearest = hi;
        }
        ++hi;
    }
    DBG("Nearest Hex to " << pos.first << "," << pos.second << " is (r,g):" << nearest->ri << "," << nearest->gi << " (x,y):" << nearest->x << "," << nearest->y);
    return nearest;
}

void
morph::HexGrid::renumberVectorIndices (void)
{
    unsigned int vi = 0;
    this->vhexen.clear();
    auto hi = this->hexen.begin();
    while (hi != this->hexen.end()) {
        hi->vi = vi++;
        this->vhexen.push_back (&(*hi));
        ++hi;
    }
}

unsigned int
morph::HexGrid::num (void) const
{
    return this->hexen.size();
}

unsigned int
morph::HexGrid::lastVectorIndex (void) const
{
    return this->hexen.rbegin()->vi;
}

string
morph::HexGrid::output (void) const
{
    stringstream ss;
    ss << "Hex grid with " << this->hexen.size() << " hexes." << endl;
    auto i = this->hexen.begin();
    float lasty = this->hexen.front().y;
    unsigned int rownum = 0;
    ss << endl << "Row/Ring " << rownum++ << ":" << endl;
    while (i != this->hexen.end()) {
        if (i->y > lasty) {
            ss << endl << "Row/Ring " << rownum++ << ":" << endl;
            lasty = i->y;
        }
        ss << i->output() << endl;
        ++i;
    }
    return ss.str();
}

string
morph::HexGrid::extent (void) const
{
    stringstream ss;
    if (gridReduced == false) {
        ss << "Grid vertices: \n"
           << "           NW: (" << this->vertexNW->x << "," << this->vertexNW->y << ") "
           << "      NE: (" << this->vertexNE->x << "," << this->vertexNE->y << ")\n"
           << "     W: (" << this->vertexW->x << "," << this->vertexW->y << ") "
           << "                              E: (" << this->vertexE->x << "," << this->vertexE->y << ")\n"
           << "           SW: (" << this->vertexSW->x << "," << this->vertexSW->y << ") "
           << "      SE: (" << this->vertexSE->x << "," << this->vertexSE->y << ")";
    } else {
        ss << "Initial grid vertices are no longer valid.";
    }
    return ss.str();
}

float
morph::HexGrid::getd (void) const
{
    return this->d;
}

float
morph::HexGrid::getv (void) const
{
    return this->v;
}

float
morph::HexGrid::getXmin (float phi) const
{
    float xmin = 0.0f;
    float x_ = 0.0f;
    bool first = true;
    for (auto h : this->hexen) {
        x_ = h.x * cos (phi) + h.y * sin (phi);
        if (first) {
            xmin = x_;
            first = false;
        }
        if (x_ < xmin) {
            xmin = x_;
        }
    }
    return xmin;
}

float
morph::HexGrid::getXmax (float phi) const
{
    float xmax = 0.0f;
    float x_ = 0.0f;
    bool first = true;
    for (auto h : this->hexen) {
        x_ = h.x * cos (phi) + h.y * sin (phi);
        if (first) {
            xmax = x_;
            first = false;
        }
        if (x_ > xmax) {
            xmax = x_;
        }
    }
    return xmax;
}

void
morph::HexGrid::init (float d_, float x_span_, float z_)
{
    this->d = d_;
    this->v = this->d * SQRT_OF_3_OVER_2_F;
    this->x_span = x_span_;
    this->z = z_;
    this->init();
}

void
morph::HexGrid::init (void)
{
    // Use span_x to determine how many rings out to traverse.
    float halfX = this->x_span/2.0f;
    DBG ("halfX:" << halfX);
    DBG ("d:" << d);
    unsigned int maxRing = abs(ceil(halfX/this->d));
    DBG ("ceil(halfX/d):" << ceil(halfX/d));

    DBG ("Creating hexagonal hex grid with maxRing: " << maxRing);

    // The "vector iterator" - this is an identity iterator that is
    // added to each Hex in the grid.
    unsigned int vi = 0;

    // Vectors of list-iterators to hexes in this->hexen. Used to keep a
    // track of nearest neighbours. I'm using vector, rather than a
    // list as this allows fast random access of elements and I'll not
    // be inserting or erasing in the middle of the arrays.
    vector<list<Hex>::iterator> prevRingEven;
    vector<list<Hex>::iterator> prevRingOdd;

    // Swap pointers between rings.
    vector<list<Hex>::iterator>* prevRing = &prevRingEven;
    vector<list<Hex>::iterator>* nextPrevRing = &prevRingOdd;

    // Direction iterators used in the loop for creating hexes
    int ri = 0;
    int gi = 0;

    // Create central "ring" first (the single hex)
    this->hexen.emplace_back (vi++, this->d, ri, gi);

    // Put central ring in the prevRing vector:
    {
        list<Hex>::iterator h = this->hexen.end(); --h;
        prevRing->push_back (h);
    }

    // Now build up the rings around it, setting neighbours as we
    // go. Each ring has 6 more hexes than the previous one (except
    // for ring 1, which has 6 instead of 1 in the centre).
    unsigned int numInRing = 6;

    // How many hops in the same direction before turning a corner?
    // Increases for each ring. Increases by 1 in each ring.
    unsigned int ringSideLen = 1;

    // These are used to iterate along the six sides of the hexagonal
    // ring that's inside, but adjacent to the hexagonal ring that's
    // under construction.
    int walkstart = 0;
    int walkinc = 0;
    int walkmin = walkstart-1;
    int walkmax = 1;

    for (unsigned int ring = 1; ring <= maxRing; ++ring) {

        DBG2 ("\n\n************** numInRing: " << numInRing << " ******************");

        // Set start ri, gi. This moves up a hex and left a hex onto
        // the start hex of the new ring.
        --ri; ++gi;

        nextPrevRing->clear();

        // Now walk around the ring, in 6 walks, that will bring us
        // round to just before we started. walkstart has the starting
        // iterator number for the vertices of the hexagon.
        DBG2 ("Before r; walkinc: " << walkinc << ", walkmin: " << walkmin << ", walkmax: " << walkmax);

        // Walk in the r direction first:
        DBG2 ("============ r walk =================");
        for (unsigned int i = 0; i<ringSideLen; ++i) {

            DBG2 ("Adding hex at " << ri << "," << gi);
            this->hexen.emplace_back (vi++, this->d, ri++, gi);
            auto hi = this->hexen.end(); hi--;
            auto lasthi = hi;
            --lasthi;

            // Set vertex
            if (i==0) { vertexNW = hi; }

            // 1. Set my W neighbour to be the previous hex in THIS ring, if possible
            if (i > 0) {
                hi->set_nw (lasthi);
                DBG2 (" r walk: Set me (" << hi->ri << "," << hi->gi << ") as E neighbour for hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as E neighbour to previous hex in the ring:
                lasthi->set_ne (hi);
            } else {
                // i must be 0 in this case, we would set the SW
                // neighbour now, but as this won't have been added to
                // the ring, we have to leave it.
                DBG2 (" r walk: I am (" << hi->ri << "," << hi->gi << "). Omitting SW neighbour of first hex in ring.");
            }

            // 2. SW neighbour
            int j = walkstart + (int)i - 1;
            DBG2 ("i is " << i << ", j is " << j << ", walk min/max: " << walkmin << " " << walkmax);
            if (j>walkmin && j<walkmax) {
                // Set my SW neighbour:
                hi->set_nsw ((*prevRing)[j]);
                // Set me as NE neighbour to those in prevRing:
                DBG2 (" r walk: Set me (" << hi->ri << "," << hi->gi << ") as NE neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nne (hi);
            }
            ++j;
            DBG2 ("i is " << i << ", j is " << j);

            // 3. Set my SE neighbour:
            if (j<=walkmax) {
                hi->set_nse ((*prevRing)[j]);
                // Set me as NW neighbour:
                DBG2 (" r walk: Set me (" << hi->ri << "," << hi->gi << ") as NW neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nnw (hi);
            }

            // Put in me nextPrevRing:
            nextPrevRing->push_back (hi);
        }
        walkstart += walkinc;
        walkmin   += walkinc;
        walkmax   += walkinc;

        // Walk in -b direction
        DBG2 ("Before -b; walkinc: " << walkinc << ", walkmin: " << walkmin << ", walkmax: " << walkmax);
        DBG2 ("=========== -b walk =================");
        for (unsigned int i = 0; i<ringSideLen; ++i) {
            DBG2 ("Adding hex at " << ri << "," << gi);
            this->hexen.emplace_back (vi++, this->d, ri++, gi--);
            auto hi = this->hexen.end(); hi--;
            auto lasthi = hi;
            --lasthi;

            // Set vertex
            if (i==0) { vertexNE = hi; }

            // 1. Set my NW neighbour to be the previous hex in THIS ring, if possible
            if (i > 0) {
                hi->set_nnw (lasthi);
                DBG2 ("-b walk: Set me (" << hi->ri << "," << hi->gi << ") as SE neighbour for hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as SE neighbour to previous hex in the ring:
                lasthi->set_nse (hi);
            } else {
                // Set my W neighbour for the first hex in the row.
                hi->set_nw (lasthi);
                DBG2 ("-b walk: Set me (" << hi->ri << "," << hi->gi << ") as E neighbour for last walk's hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as E neighbour to previous hex in the ring:
                lasthi->set_ne (hi);
            }

            // 2. W neighbour
            int j = walkstart + (int)i - 1;
            DBG2 ("i is " << i << ", j is " << j << " prevRing->size(): " <<prevRing->size() );
            if (j>walkmin && j<walkmax) {
                // Set my W neighbour:
                hi->set_nw ((*prevRing)[j]);
                // Set me as E neighbour to those in prevRing:
                DBG2 ("-b walk: Set me (" << hi->ri << "," << hi->gi << ") as E neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_ne (hi);
            }
            ++j;
            DBG2 ("i is " << i << ", j is " << j);

            // 3. Set my SW neighbour:
            DBG2 ("i is " << i << ", j is " << j);
            if (j<=walkmax) {
                hi->set_nsw ((*prevRing)[j]);
                // Set me as NE neighbour:
                DBG2 ("-b walk: Set me (" << hi->ri << "," << hi->gi << ") as NE neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nne (hi);
            }

            nextPrevRing->push_back (hi);
        }
        walkstart += walkinc;
        walkmin += walkinc;
        walkmax += walkinc;
        DBG2 ("walkinc: " << walkinc << ", walkmin: " << walkmin << ", walkmax: " << walkmax);

        // Walk in -g direction
        DBG2 ("=========== -g walk =================");
        for (unsigned int i = 0; i<ringSideLen; ++i) {

            DBG2 ("Adding hex at " << ri << "," << gi);
            this->hexen.emplace_back (vi++, this->d, ri, gi--);
            auto hi = this->hexen.end(); hi--;
            auto lasthi = hi;
            --lasthi;

            // Set vertex
            if (i==0) { vertexE = hi; }

            // 1. Set my NE neighbour to be the previous hex in THIS ring, if possible
            if (i > 0) {
                hi->set_nne (lasthi);
                DBG2 ("-g walk: Set me (" << hi->ri << "," << hi->gi << ") as SW neighbour for hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as SW neighbour to previous hex in the ring:
                lasthi->set_nsw (hi);
            } else {
                // Set my NW neighbour for the first hex in the row.
                hi->set_nnw (lasthi);
                DBG2 ("-g walk: Set me (" << hi->ri << "," << hi->gi << ") as SE neighbour for last walk's hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as SE neighbour to previous hex in the ring:
                lasthi->set_nse (hi);
            }

            // 2. NW neighbour
            int j = walkstart + (int)i - 1;
            DBG2 ("i is " << i << ", j is " << j);
            if (j>walkmin && j<walkmax) {
                // Set my NW neighbour:
                hi->set_nnw ((*prevRing)[j]);
                // Set me as SE neighbour to those in prevRing:
                DBG2 ("-g walk: Set me (" << hi->ri << "," << hi->gi << ") as SE neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nse (hi);
            }
            ++j;
            DBG2 ("i is " << i << ", j is " << j);

            // 3. Set my W neighbour:
            if (j<=walkmax) {
                hi->set_nw ((*prevRing)[j]);
                // Set me as E neighbour:
                DBG2 ("-g walk: Set me (" << hi->ri << "," << hi->gi << ") as E neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_ne (hi);
            }

            // Put in me nextPrevRing:
            nextPrevRing->push_back (hi);
        }
        walkstart += walkinc;
        walkmin += walkinc;
        walkmax += walkinc;
        DBG2 ("walkinc: " << walkinc << ", walkmin: " << walkmin << ", walkmax: " << walkmax);

        // Walk in -r direction
        DBG2 ("=========== -r walk =================");
        for (unsigned int i = 0; i<ringSideLen; ++i) {

            DBG2 ("Adding hex at " << ri << "," << gi);
            this->hexen.emplace_back (vi++, this->d, ri--, gi);
            auto hi = this->hexen.end(); hi--;
            auto lasthi = hi;
            --lasthi;

            // Set vertex
            if (i==0) { vertexSE = hi; }

            // 1. Set my E neighbour to be the previous hex in THIS ring, if possible
            if (i > 0) {
                hi->set_ne (lasthi);
                DBG2 ("-r walk: Set me (" << hi->ri << "," << hi->gi << ") as W neighbour for hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as W neighbour to previous hex in the ring:
                lasthi->set_nw (hi);
            } else {
                // Set my NE neighbour for the first hex in the row.
                hi->set_nne (lasthi);
                DBG2 ("-r walk: Set me (" << hi->ri << "," << hi->gi << ") as SW neighbour for last walk's hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as SW neighbour to previous hex in the ring:
                lasthi->set_nsw (hi);
            }

            // 2. NE neighbour:
            int j = walkstart + (int)i - 1;
            DBG2 ("i is " << i << ", j is " << j);
            if (j>walkmin && j<walkmax) {
                // Set my NE neighbour:
                hi->set_nne ((*prevRing)[j]);
                // Set me as SW neighbour to those in prevRing:
                DBG2 ("-r walk: Set me (" << hi->ri << "," << hi->gi << ") as SW neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nsw (hi);
            }
            ++j;
            DBG2 ("i is " << i << ", j is " << j);

            // 3. Set my NW neighbour:
            if (j<=walkmax) {
                hi->set_nnw ((*prevRing)[j]);
                // Set me as SE neighbour:
                DBG2 ("-r walk: Set me (" << hi->ri << "," << hi->gi << ") as SE neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nse (hi);
            }

            nextPrevRing->push_back (hi);
        }
        walkstart += walkinc;
        walkmin += walkinc;
        walkmax += walkinc;
        DBG2 ("walkinc: " << walkinc << ", walkmin: " << walkmin << ", walkmax: " << walkmax);

        // Walk in b direction
        DBG2 ("============ b walk =================");
        for (unsigned int i = 0; i<ringSideLen; ++i) {
            DBG2 ("Adding hex at " << ri << "," << gi);
            this->hexen.emplace_back (vi++, this->d, ri--, gi++);
            auto hi = this->hexen.end(); hi--;
            auto lasthi = hi;
            --lasthi;

            // Set vertex
            if (i==0) { vertexSW = hi; }

            // 1. Set my SE neighbour to be the previous hex in THIS ring, if possible
            if (i > 0) {
                hi->set_nse (lasthi);
                DBG2 (" b in-ring: Set me (" << hi->ri << "," << hi->gi << ") as NW neighbour for hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as NW neighbour to previous hex in the ring:
                lasthi->set_nnw (hi);
            } else { // i == 0
                // Set my E neighbour for the first hex in the row.
                hi->set_ne (lasthi);
                DBG2 (" b in-ring: Set me (" << hi->ri << "," << hi->gi << ") as W neighbour for last walk's hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as W neighbour to previous hex in the ring:
                lasthi->set_nw (hi);
            }

            // 2. E neighbour:
            int j = walkstart + (int)i - 1;
            DBG2 ("i is " << i << ", j is " << j);
            if (j>walkmin && j<walkmax) {
                // Set my E neighbour:
                hi->set_ne ((*prevRing)[j]);
                // Set me as W neighbour to those in prevRing:
                DBG2 (" b walk: Set me (" << hi->ri << "," << hi->gi << ") as W neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nw (hi);
            }
            ++j;
            DBG2 ("i is " << i << ", j is " << j);

            // 3. Set my NE neighbour:
            if (j<=walkmax) {
                hi->set_nne ((*prevRing)[j]);
                // Set me as SW neighbour:
                DBG2 (" b walk: Set me (" << hi->ri << "," << hi->gi << ") as SW neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nsw (hi);
            }

            nextPrevRing->push_back (hi);
        }
        walkstart += walkinc;
        walkmin += walkinc;
        walkmax += walkinc;
        DBG2 ("walkinc: " << walkinc << ", walkmin: " << walkmin << ", walkmax: " << walkmax);

        // Walk in g direction up to almost the last hex
        DBG2 ("============ g walk =================");
        for (unsigned int i = 0; i<ringSideLen; ++i) {

            DBG2 ("Adding hex at " << ri << "," << gi);
            this->hexen.emplace_back (vi++, this->d, ri, gi++);
            auto hi = this->hexen.end(); hi--;
            auto lasthi = hi;
            --lasthi;

            // Set vertex
            if (i==0) { vertexW = hi; }

            // 1. Set my SW neighbour to be the previous hex in THIS ring, if possible
            DBG2(" g walk: i is " << i << " and ringSideLen-1 is " << (ringSideLen-1));
            if (i == (ringSideLen-1)) {
                // Special case at end; on last g walk hex, set the NE neighbour
                // Set my NE neighbour for the first hex in the row.
                hi->set_nne ((*nextPrevRing)[0]); // (*nextPrevRing)[0] is an iterator to the first hex

                DBG2 (" g in-ring: Set me (" << hi->ri << "," << hi->gi << ") as SW neighbour for this ring's first hex at (" << (*nextPrevRing)[0]->ri << "," << (*nextPrevRing)[0]->gi << ")");
                // Set me as NW neighbour to previous hex in the ring:
                (*nextPrevRing)[0]->set_nsw (hi);

            }
            if (i > 0) {
                hi->set_nsw (lasthi);
                DBG2 (" g in-ring: Set me (" << hi->ri << "," << hi->gi << ") as NE neighbour for hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as NE neighbour to previous hex in the ring:
                lasthi->set_nne (hi);
            } else {
                // Set my SE neighbour for the first hex in the row.
                hi->set_nse (lasthi);
                DBG2 (" g in-ring: Set me (" << hi->ri << "," << hi->gi << ") as NW neighbour for last walk's hex at (" << lasthi->ri << "," << lasthi->gi << ")");
                // Set me as NW neighbour to previous hex in the ring:
                lasthi->set_nnw (hi);
            }

            // 2. E neighbour:
            int j = walkstart + (int)i - 1;
            DBG2 ("i is " << i << ", j is " << j);
            if (j>walkmin && j<walkmax) {
                // Set my SE neighbour:
                hi->set_nse ((*prevRing)[j]);
                // Set me as NW neighbour to those in prevRing:
                DBG2 (" g walk: Set me (" << hi->ri << "," << hi->gi << ") as NW neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nnw (hi);
            }
            ++j;
            DBG2 ("i is " << i << ", j is " << j);

            // 3. Set my E neighbour:
            if (j==walkmax) { // We're on the last square and need to
                              // set the East neighbour of the first
                              // hex in the last ring.
                hi->set_ne ((*prevRing)[0]);
                // Set me as W neighbour:
                DBG2 (" g walk: Set me (" << hi->ri << "," << hi->gi << ") as W neighbour for hex at (" << (*prevRing)[0]->ri << "," << (*prevRing)[0]->gi << ")");
                (*prevRing)[0]->set_nw (hi);

            } else if (j<walkmax) {
                hi->set_ne ((*prevRing)[j]);
                // Set me as W neighbour:
                DBG2 (" g walk: Set me (" << hi->ri << "," << hi->gi << ") as W neighbour for hex at (" << (*prevRing)[j]->ri << "," << (*prevRing)[j]->gi << ")");
                (*prevRing)[j]->set_nw (hi);
            }

            // Put in me nextPrevRing:
            nextPrevRing->push_back (hi);
        }
        // Should now be on the last hex.

        // Update the walking increments for finding the vertices of
        // the hexagonal ring. These are for walking around the ring
        // *inside* the ring of hexes being created and hence note
        // that I set walkinc to numInRing/6 BEFORE incrementing
        // numInRing by 6, below.
        walkstart = 0;
        walkinc = numInRing / 6;
        walkmin = walkstart - 1;
        walkmax = walkmin + 1 + walkinc;

        // Always 6 additional hexes in the next ring out
        numInRing += 6;

        // And ring side length goes up by 1
        ringSideLen++;

        // Swap prevRing and nextPrevRing.
        vector<list<Hex>::iterator>* tmp = prevRing;
        prevRing = nextPrevRing;
        DBG2 ("New prevRing contains " << prevRing->size() << " elements");
        nextPrevRing = tmp;
    }

    DBG ("Finished creating " << this->hexen.size() << " hexes in " << maxRing << " rings.");
}
