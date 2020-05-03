/*!
 * \file
 * \brief An N dimensional vector class template which derives from std::array.
 *
 * \author Seb James (with thanks to Konrad Rudolph and Miguel Avila for code review)
 * \date April 2020
 */
#pragma once

#include <cmath>
#include <array>
#include <iostream>
#include <string>
#include <sstream>
#include <type_traits>
#include <numeric>
#include <algorithm>
#include <functional>
#include "Random.h"

namespace morph {

    /*!
     * \brief N-D vector class
     *
     * An N dimensional vector class template which derives from std::array. Vector
     * components are of scalar type S. It is anticipated that S will be set either to
     * floating point scalar types such as float or double, or to integer scalar types
     * such as int, long long int and so on. Thus, a typical (and in fact, the default)
     * signature would be:
     *
     * Vector<float, 3> v;
     *
     * The class inherits std:array's fixed-size array of memory for storing the
     * components of the vector. It adds numerous methods which allow objects of type
     * Vector to have arithmetic operations applied to them, either scalar (add a scalar
     * to all elements; divide all elements by a scalar, etc) or vector (including dot
     * and cross products, normalization and so on.
     *
     * Because morph::Vector extends std::array, it works best when compiled with a
     * c++-17 compiler (although it can be compiled with a c++-11 compiler). This is
     * because std::array is an 'aggregate class' with no user-provided constructors,
     * and morph::Vector does not add any of its own constructors. Prior to c++-17,
     * aggregate classes were not permitted to have base classes. So, if you want to do:
     *
     * Vector<float, 3> v = { 1.0f , 1.0f, 1.0f };
     *
     * You need c++-17. Otherwise, restrict your client code to doing:
     *
     * Vector<float, 3> v;
     * v[0] = 1.0f; v[1] = 1.0f; v[2] = 1.0f;
     */
    template <typename S, size_t N> struct Vector;

    /*!
     * Template friendly mechanism to overload the stream operator.
     *
     * Note forward declaration of the Vector template class and this template for
     * stream operator overloading. Example adapted from
     * https://stackoverflow.com/questions/4660123
     */
    template <typename S, size_t N> std::ostream& operator<< (std::ostream&, const Vector<S, N>&);

    template <typename S=float, size_t N=3>
    struct Vector : public std::array<S, N>
    {
        //! \return the first component of the vector
        template <size_t _N = N, std::enable_if_t<(_N>0), int> = 0>
        S x() const {
            return (*this)[0];
        }
        //! \return the second component of the vector
        template <size_t _N = N, std::enable_if_t<(_N>1), int> = 0>
        S y() const {
            return (*this)[1];
        }
        //! \return the third component of the vector
        template <size_t _N = N, std::enable_if_t<(_N>2), int> = 0>
        S z() const {
            return (*this)[2];
        }
        //! \return the fourth component of the vector
        template <size_t _N = N, std::enable_if_t<(_N>3), int> = 0>
        S w() const {
            return (*this)[3];
        }

        /*!
         * \brief Unit vector threshold
         *
         * The threshold outside of which the vector is no longer considered to be a
         * unit vector. Note this is hard coded as a constexpr, to avoid messing with
         * the initialization of the Vector with curly brace initialization.
         *
         * Clearly, this will be the wrong threshold for some cases. Possibly, a
         * template parameter could set this; so size_t U could indicate the threshold;
         * 0.001 could be U=-3 (10^-3).
         *
         * Another idea would be to change unitThresh based on the type S. Or use
         * numeric_limits<S>::epsilon and find out what multiple of epsilon would make
         * sense.
         */
        static constexpr S unitThresh = 0.001;

        /*!
         * Set data members from an array the of same size and type.
         */
        void set_from (const std::array<S, N>& ar) {
            std::copy (ar.begin(), ar.end(), this->begin());
        }

        /*!
         * Set the data members of this Vector from the passed in, larger array, \a ar,
         * ignoring the last element of \a ar. Used when working with 4D vectors in
         * graphics applications involving 4x4 transform matrices.
         */
        void set_from (const std::array<S, (N+1)>& ar) {
            // Don't use std::copy here, because ar has more elements than *this.
            for (size_t i = 0; i < N; ++i) {
                (*this)[i] = ar[i];
            }
        }

        /*!
         * Set an N-D Vector from an N+1 D Vector. Intended to convert 4D vectors (that
         * have been operated on by 4x4 matrices) into 3D vectors.
         */
        void set_from (const Vector<S, (N+1)>& v) {
            for (size_t i = 0; i < N; ++i) {
                (*this)[i] = v[i];
            }
        }

        /*!
         * Create a string representation of the vector
         *
         * \return A 'coordinate format' string such as "(1,1,2)", "(0.2,0.4)" or
         * "(5,4,5,5,40)".
         */
        std::string str() const {
            std::stringstream ss;
            ss << "(";
            bool first = true;
            for (auto i : *this) {
                if (first) {
                    ss << i;
                    first = false;
                } else {
                    ss << "," << i;
                }
            }
            ss << ")";
            return ss.str();
        }

        /*!
         * Renormalize the vector to length 1.0. Only for S types that are floating point.
         */
        template <typename F=S, std::enable_if_t<!std::is_integral<std::decay_t<F>>::value, int> = 0 >
        void renormalize() {
            auto add_squared = [](F a, F b) { return a + b * b; };
            const F denom = std::sqrt (std::accumulate (this->begin(), this->end(), F{0}, add_squared));
            if (denom != F{0}) {
                F oneovermag = F{1} / denom;
                auto x_oneovermag = [oneovermag](F f) { return f * oneovermag; };
                std::transform (this->begin(), this->end(), this->begin(), x_oneovermag);
            }
        }

        /*!
         * Zero the vector. Set all coordinates to 0
         */
        void zero() {
            std::fill (this->begin(), this->end(), S{0});
        }

        /*!
         * Randomize the vector
         *
         * Randomly set the elements of the vector consisting of floating point
         * coordinates. Coordinates are set to random numbers drawn from a uniform
         * distribution between 0 and 1 (See morph::RandUniformReal for details).
         *
         * Note that I need a real or int implementation here, depending on the type of
         * S. This allows me to use the correct type of randomizer.
         *
         * Note, if you omit the second template arg from enable_if_t (or enable_if)
         * then the type defaults to void.
         *
         * \tparam F A floating point scalar type
         */
        template <typename F=S, std::enable_if_t<!std::is_integral<std::decay_t<F>>::value, int> = 0 >
        void randomize() {
            RandUniformReal<F> ruf (static_cast<F>(0), static_cast<F>(1));
            for (auto& i : *this) {
                i = ruf.get();
            }
        }

        /*!
         * Randomize the vector
         *
         * Randomly set the elements of the vector consisting of integer
         * coordinates. Coordinates are set to random numbers drawn from a uniform
         * distribution between 0 and 255 (See morph::RandUniformInt for details).
         *
         * Note on the template syntax: Here, if I is integral, then enable_if_t's type
         * is '0' and the function is defined (I think).
         *
         * \tparam I An integer scalar type
         */
        template <typename I=S, std::enable_if_t<std::is_integral<std::decay_t<I>>::value, int> = 0 >
        void randomize() {
            RandUniformInt<I> rui (static_cast<I>(0), static_cast<I>(255));
            for (auto& i : *this) {
                i = rui.get();
            }
        }

        /*!
         * Test to see if this vector is a unit vector (it doesn't *have* to be).
         *
         * \return true if the length of the vector is 1.
         */
        bool checkunit() const {
            auto subtract_squared = [](S a, S b) { return a - b * b; };
            const S metric = std::accumulate (this->begin(), this->end(), S{1}, subtract_squared);
            if (std::abs(metric) > Vector<S, N>::unitThresh) {
                return false;
            }
            return true;
        }

        /*!
         * Find the length of the vector.
         *
         * \return the length
         */
        S length() const {
            auto add_squared = [](S a, S b) { return a + b * b; };
            const S len = std::sqrt (std::accumulate (this->begin(), this->end(), S{0}, add_squared));
            return len;
        }

        /*!
         * Unary negate operator
         *
         * \return a Vector whose elements have been negated.
         */
        Vector<S, N> operator-() const {
            Vector<S, N> rtn;
            std::transform (this->begin(), this->end(), rtn.begin(), std::negate<S>());
            return rtn;
        }

        /*!
         * Unary not operator.
         *
         * \return true if the vector length is 0, otherwise it returns false.
         */
        bool operator!() const {
            return (this->length() == S{0}) ? true : false;
        }

        /*!
         * Vector multiply * operator.
         *
         * Cross product of this with another vector \a v (if N==3). In
         * higher dimensions, its more complicated to define what the cross product is,
         * and I'm unlikely to need anything other than the plain old 3D cross product.
         */
        template <size_t _N = N, std::enable_if_t<(_N==3), int> = 0>
        Vector<S, N> operator* (const Vector<S, _N>& v) const {
            Vector<S, _N> vrtn;
            vrtn[0] = (*this)[1] * v.z() - (*this)[2] * v.y();
            vrtn[1] = (*this)[2] * v.x() - (*this)[0] * v.z();
            vrtn[2] = (*this)[0] * v.y() - (*this)[1] * v.x();
            return vrtn;
        }

        /*!
         * Vector multiply *= operator.
         *
         * Cross product of this with another vector v (if N==3). Result written into
         * this.
         */
        template <size_t _N = N, std::enable_if_t<(_N==3), int> = 0>
        void operator*= (const Vector<S, _N>& v) {
            Vector<S, _N> vtmp;
            vtmp[0] = (*this)[1] * v.z() - (*this)[2] * v.y();
            vtmp[1] = (*this)[2] * v.x() - (*this)[0] * v.z();
            vtmp[2] = (*this)[0] * v.y() - (*this)[1] * v.x();
            std::copy (vtmp.begin(), vtmp.end(), this->begin());
        }

        /*!
         * \brief Scalar (dot) product
         *
         * Compute the scalar product of this Vector and the Vector, v.
         *
         * \return scalar product
         */
        S dot (const Vector<S, N>& v) const {
            auto vi = v.begin();
            auto dot_product = [vi](S a, S b) mutable { return a + b * (*vi++); };
            const S rtn = std::accumulate (this->begin(), this->end(), S{0}, dot_product);
            return rtn;
        }

        /*!
         * Scalar multiply * operator
         *
         * This function will only be defined if typename _S is a
         * scalar type. Multiplies this Vector<S, N> by s, element-wise.
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        Vector<_S, N> operator* (const _S& s) const {
            Vector<_S, N> rtn;
            auto mult_by_s = [s](_S coord) { return coord * s; };
            std::transform (this->begin(), this->end(), rtn.begin(), mult_by_s);
            return rtn;
        }

        /*!
         * Scalar multiply *= operator
         *
         * This function will only be defined if typename _S is a
         * scalar type. Multiplies this Vector<S, N> by s, element-wise.
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        void operator*= (const _S& s) {
            auto mult_by_s = [s](_S coord) { return coord * s; };
            std::transform (this->begin(), this->end(), this->begin(), mult_by_s);
        }

        /*!
         * Scalar divide by s
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        Vector<_S, N> operator/ (const _S& s) const {
            Vector<_S, N> rtn;
            auto div_by_s = [s](_S coord) { return coord / s; };
            std::transform (this->begin(), this->end(), rtn.begin(), div_by_s);
            return rtn;
        }

        /*!
         * Scalar divide by s
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        void operator/= (const _S& s) {
            auto div_by_s = [s](_S coord) { return coord / s; };
            std::transform (this->begin(), this->end(), this->begin(), div_by_s);
        }

        /*!
         * Vector addition operator
         */
        Vector<S, N> operator+ (const Vector<S, N>& v) const {
            Vector<S, N> vrtn;
            auto vi = v.begin();
            auto add_v = [vi](S a) mutable { return a + (*vi++); };
            std::transform (this->begin(), this->end(), vrtn.begin(), add_v);
            return vrtn;
        }

        /*!
         * Vector addition operator
         */
        void operator+= (const Vector<S, N>& v) {
            auto vi = v.begin();
            auto add_v = [vi](S a) mutable { return a + (*vi++); };
            std::transform (this->begin(), this->end(), this->begin(), add_v);
        }

        /*!
         * Vector subtraction operator
         */
        Vector<S, N> operator- (const Vector<S, N>& v) const {
            Vector<S, N> vrtn;
            auto vi = v.begin();
            auto subtract_v = [vi](S a) mutable { return a - (*vi++); };
            std::transform (this->begin(), this->end(), vrtn.begin(), subtract_v);
            return vrtn;
        }

        /*!
         * Vector subtraction operator
         */
        void operator-= (const Vector<S, N>& v) {
            auto vi = v.begin();
            auto subtract_v = [vi](S a) mutable { return a - (*vi++); };
            std::transform (this->begin(), this->end(), this->begin(), subtract_v);
        }

        /*!
         * Scalar addition
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        Vector<_S, N> operator+ (const _S& s) const {
            Vector<_S, N> rtn;
            auto add_s = [s](_S coord) { return coord + s; };
            std::transform (this->begin(), this->end(), rtn.begin(), add_s);
            return rtn;
        }

        /*!
         * Scalar addition
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        void operator+= (const _S& s) {
            auto add_s = [s](_S coord) { return coord + s; };
            std::transform (this->begin(), this->end(), this->begin(), add_s);
        }

        /*!
         * Scalar subtraction
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        Vector<_S, N> operator- (const _S& s) const {
            Vector<_S, N> rtn;
            auto subtract_s = [s](_S coord) { return coord - s; };
            std::transform (this->begin(), this->end(), rtn.begin(), subtract_s);
            return rtn;
        }

        /*!
         * Scalar subtraction
         */
        template <typename _S=S, std::enable_if_t<std::is_scalar<std::decay_t<_S>>::value, int> = 0 >
        void operator-= (const _S& s) {
            auto subtract_s = [s](_S coord) { return coord - s; };
            std::transform (this->begin(), this->end(), this->begin(), subtract_s);
        }

        /*!
         * Overload the stream output operator
         */
        friend std::ostream& operator<< <S, N> (std::ostream& os, const Vector<S, N>& v);
    };

    template <typename S=float, size_t N=3>
    std::ostream& operator<< (std::ostream& os, const Vector<S, N>& v)
    {
        os << v.str();
        return os;
    }

} // namespace morph
