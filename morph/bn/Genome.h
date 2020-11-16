/*!
 * Snazzy templated Genome class derived from std::array.
 *
 * Author: Seb James
 * Date: Nov 2020
 */

#pragma once

#include <array>
#include <list>
#include <bitset>
#include <iostream>
#include <sstream>
#include <immintrin.h>
#include <morph/Random.h>

namespace morph {
    namespace  bn {

        /*!
         * Genosect is a template metafunction, with several specializations. It has one
         * attribute, type, which client code should use as the correct type for the
         * Genome's array. Each Genome section in the Genome's array has 2^K bits.
         */
        template <size_t K = 0> struct Genosect {}; // Default should fail to compile
        template<> struct Genosect<1> { typedef unsigned char type; };
        template<> struct Genosect<2> { typedef unsigned char type; };
        template<> struct Genosect<3> { typedef unsigned char type; };
        template<> struct Genosect<4> { typedef unsigned short type; };
        template<> struct Genosect<5> { typedef unsigned int type; };
        template<> struct Genosect<6> { typedef unsigned long long int type; };

        // A Genome is (derived from) an array<Genosect<K>::type, N>.
        template <size_t N, size_t K> struct Genome;
        template <size_t N, size_t K> std::ostream& operator<< (std::ostream&, const Genome<N, K>&);

        /*!
         * The Genome class
         *
         * In our bn namespace, a Genome for a Boolean gene network of N 'genes' has N
         * 'genosects' stored in an array (which is why this class derives from
         * std::array). The number of effective inputs to the network, K, also has to be
         * provided. This 'n-k' terminology matches that used by Stuart Kaufmann in his
         * discussion of Boolean nets.
         *
         * \tparam T The width of each 'genosect'. This should be an unsigned integral
         * type; unsigned char, unsigned int or unsigned long long int.
         *
         * \tparam N The number of genes in the Boolean gene network
         *
         * \tparam K The number of genes which are used to determine the next state of
         * the Boolean gene net. May not be greater than N.
         */
        template <size_t N=5, size_t K=5>
        struct Genome : public std::array<typename Genosect<K>::type, N>
        {
            using genosect_t = typename Genosect<K>::type;

            //! Compile-time function used to initialize genosect_mask, the mask used to
            //! get the significant bits of a genome section.
            static constexpr genosect_t genosect_mask_init()
            {
                genosect_t _genosect_mask = 0x0;
                for (unsigned int i = 0; i < (1<<K); ++i) {
                    _genosect_mask |= (genosect_t{1} << i);
                }
                return _genosect_mask;
            }
            static constexpr genosect_t genosect_mask = Genome::genosect_mask_init();

            //! Provide a check that K is not > N. The static_assert ensures that compile will fail if K>N
            static constexpr bool checkTemplateParams()
            {
                static_assert(K <= N);
                return (K <= N ? true : false);
            }

            //! String output
            std::string str() const
            {
                std::stringstream ss;
                ss << std::hex;
                bool first = true;
                for (unsigned int i = 0; i<N; ++i) {
                    if (first) {
                        first = false;
                        ss << ((*this)[i] & this->genosect_mask);
                    } else {
                        ss << "-" << ((*this)[i] & this->genosect_mask);
                    }
                }
                ss << std::dec;
                return ss.str();
            }

            //! A debugging aid to display the genome in a little table.
            std::string table() const
            {
                std::stringstream ss;
                ss << "Genome:" << std::endl;
                bool first = true;
                for (unsigned int i = 0; i<N; ++i) {
                    if (first) {
                        first = false;
                        ss << (char)('a'+i);
                    } else {
                        ss << "     " << (char)('a'+i);
                    }
                }
                ss << std::endl << std::hex;
                first = true;
                for (unsigned int i = 0; i<N; ++i) {
                    if (first) {
                        first = false;
                        ss << "0x" << (*this)[i];
                    } else {
                        ss << " 0x" << (*this)[i];
                    }
                }
                ss << std::dec << std::endl;
                ss << "Genome table:" << std::endl;
                ss << "input  output" << std::endl;
                for (unsigned int i = K; i > 0; --i) {
                    ss << (i-1);
                }
                ss << "   ";
                for (unsigned int i = 0; i < N; ++i) {
                    ss << i << " ";
                }
                ss << "<-- for input, bit posn; for output, array index";

                if constexpr (N==5) {
                    if constexpr (K==N) {

                        ss << std::endl << "----------------" << std::endl;
                        ss << "12345   abcde <-- 1,2,3,4,5 is i ii iii iv v in Fig 1." << std::endl;
                    } else {
                        ss << std::endl << "-----------------" << std::endl;
                        ss << "1234   abcde <-- 1,2,3,4 is i ii iii iv in Fig 1." << std::endl;
                    }
                } else {
                    for (unsigned int i = 0; i<K; ++i) { ss << i; }
                    ss << "  ";
                    for (unsigned int i = 0; i<N; ++i) { ss << " " << (char)('a'+i); }
                    ss << std::endl;
                }
                ss << "----------------" << std::endl;

                for (unsigned int j = 0; j < (1 << K); ++j) {
                    ss << std::bitset<K>(j) << "   ";
                    for (unsigned int i = 0; i < N; ++i) {
                        genosect_t mask = genosect_t{1} << j;
                        ss << (((*this)[i]&mask) >> j);
                    }
                    ss << std::endl;
                }

                return ss.str();
            }

            //! Set the genome to zero.
            void zero() { for (unsigned int i = 0; i < N; ++i) { (*this)[i] = 0; } }

            //! Evolve, but rather than flipping each bit with a certain
            //! probability, instead flip bits_to_flip bits, selected randomly.
            void evolve (unsigned int bits_to_flip)
            {
                unsigned int genosect_w = (1 << K);
                unsigned int lgenome = static_cast<unsigned int>(N) * genosect_w;

                // Init a list containing all the indices, lgenome long. As bits
                // are flipped, we'll remove from this list, ensuring that the
                // random selection of the remaining bits remains fair.
                std::list<unsigned int> idices;
                for (unsigned int b = 0; b < lgenome; ++b) { idices.push_back (b); }

                for (unsigned int b = 0; b < bits_to_flip; ++b) {
                    // FIXME: Probably want an rng which has the correct limits here, not a frng.
                    unsigned int r = static_cast<unsigned int>(std::floor(this->frng.get() * (float)lgenome));
                    // Catch the edge case (where randDouble() returned exactly 1.0)
                    if (r == lgenome) { --r; }

                    // The bit to flip is *i, after these two lines:
                    std::list<unsigned int>::iterator i = idices.begin();
                    for (unsigned int rr = 0; rr < r; ++rr, ++i) {}

                    unsigned int j = *i;
                    // Find out which genome sect j is in.
                    unsigned int gi = j / genosect_w;
                    //DBG ("Genome section gi= " << gi << " which is an offset of " << (gi*genosect_w));
                    genosect_t gsect = (*this)[gi];

                    j -= (gi*genosect_w);

                    gsect ^= (genosect_t{1} << j);
                    (*this)[gi] = gsect;

                    --lgenome;
                }
            }

            //! Evolve this genome with bit flip probability p
            void evolve (const float& p)
            {
                for (unsigned int i = 0; i < N; ++i) {
                    genosect_t gsect = (*this)[i];
                    for (unsigned int j = 0; j < (1<<K); ++j) {
                        if (this->frng.get() < p) {
                            // Flip bit j
                            gsect ^= (genosect_t{1} << j);
                        }
                    }
                    (*this)[i] = gsect;
                }
            }

            //! A version of evolve which adds to a count of the number of flips made in
            //! each genosect. For debugging.
            void evolve (const float& p, std::array<unsigned long long int, N>& flipcount)
            {
                for (unsigned int i = 0; i < N; ++i) {
                    genosect_t gsect = (*this)[i];
                    for (unsigned int j = 0; j < (1<<K); ++j) {
                        if (this->frng.get() < p) {
                            // Flip bit j
                            ++flipcount[i];
                            gsect ^= (genosect_t{1} << j);
                        }
                    }
                    (*this)[i] = gsect;
                }
            }

            //! Flip one bit in this genome at index sectidx within section sect
            void bitflip (unsigned int sect, unsigned int sectidx)
            {
                (*this)[sect] ^= (genosect_t{1} << sectidx);
            }

            //! Compute Hamming distance between this genome and another (g2).
            unsigned int hamming (const morph::bn::Genome<N,K>& g2) const
            {
                unsigned int hamming = 0;
                for (unsigned int i = 0; i < N; ++i) {
                    genosect_t bits = (*this)[i] ^ g2[i]; // XOR
                    // Compile time-selection of the correct population count intrinsic
                    if constexpr (sizeof(genosect_t) == 1) {
                        hamming += _mm_popcnt_u32 ((unsigned int)bits);
                    } else if constexpr (sizeof(genosect_t) == 2) {
                        hamming += _mm_popcnt_u32 ((unsigned int)bits);
                    } else if constexpr (sizeof(genosect_t) == 4) {
                        hamming += _mm_popcnt_u32 ((unsigned int)bits);
                        // _mm_popcnt_u32 is __popcnt() on AMD, but the above should be binary compatible
                    } else { // } else if constexpr (sizeof(genosect_t) == 8) {
                        hamming += _mm_popcnt_u64 ((unsigned long long int)bits);
                    }
                }
                return hamming;
            }

            /*!
             * Is the function defined by the genosect_t @gs a canalysing function?
             *
             * If not, return (unsigned int)0, otherwise return the number of bits for
             * which the function is canalysing - this may be called canalysing "depth".
             */
            unsigned int isCanalyzing (const genosect_t& gs) const
            {
                std::bitset<K> acanal_set;
                std::bitset<K> acanal_unset;
                std::array<int, K> setbitivalue;
                std::array<int, K> unsetbitivalue;
                unsigned int canal = 0;

                for (unsigned int i = 0; i < K; ++i) {
                    acanal_set[i] = false;
                    acanal_unset[i] = false;
                    setbitivalue[i] = -1;
                    unsetbitivalue[i] = -1;
                }

                // Test each bit. If bit's state always leads to same result in gs, then
                // canalysing is true for that bit.
                for (unsigned int i = 0; i < K; ++i) {

                    // Test bit i. First assume it IS canalysing for this bit
                    acanal_set.set(i);
                    acanal_unset.set(i);

                    // Iterate over the rows in the truth table
                    for (unsigned int j = 0; j < (0x1 << K); ++j) {

                        // if (bit i in row j is on)
                        if ((j & (genosect_t{1}<<i)) == (genosect_t{1}<<i)) {
                            if (setbitivalue[i] == -1) {
                                // -1 means we haven't yet recorded an output from gs, so record it:
                                setbitivalue[i] = (int)(genosect_t{1}&(gs>>j));
                            } else {
                                if (setbitivalue[i] != (int)(genosect_t{1}&(gs>>j))) {
                                    // Cannot be canalysing for bit i set to 1.
                                    acanal_set.reset(i);
                                }
                            }
                        } else {
                            // (bit i in row j is off)
                            if (unsetbitivalue[i] == -1) {
                                // Haven't yet recorded an output from gs, so record it:
                                unsetbitivalue[i] = (int)(genosect_t{1}&(gs>>j));
                            } else {
                                if (unsetbitivalue[i] != (int)(genosect_t{1}&(gs>>j))) {
                                    // Cannot be canalysing for bit i set to 0
                                    acanal_unset.reset(i) = false;
                                }
                            }
                        }
                    }
                }

                // Count up
                for (unsigned int i = 0; i < K; ++i) {
                    if (acanal_set.test(i) == true) {
                        canal++;
                        //DBG2 ("Bit " << i << "=1 produces a consistent output value");
                    }
                    if (acanal_unset.test(i) == true) {
                        canal++;
                        //DBG2 ("Bit " << i << "=0 produces a consistent output value");
                    }
                }

                return canal;
            }

            /*!
             * Test each section of the genosect and determine how many of the truth
             * tables are canalysing functions. Return the number of truth tables that
             * are canalysing.
             */
            unsigned int canalyzingness() const
            {
                unsigned int canal = 0;
                for (unsigned int i = 0; i < N; ++i) {
                    canal += this->isCanalyzing ((*this)[i]);
                }
                return canal;
            }

            //! Compute the bias; the proportion of set bits in the genome.
            float bias() const
            {
                unsigned int bits = 0;
                for (unsigned int i = 0; i < N; ++i) {
                    for (unsigned int j = 0; j < (0x1 << N); ++j) {
                        bits += (((*this)[i] >> j) & 0x1) ? 1 : 0;
                    }
                }
                return (float)bits/(float)(N*(1<<N));
            }

            //! This Genome has its own random number generator
            morph::RandUniform<genosect_t> rng;
            void randomize()
            {
                for (unsigned int i = 0; i < N; ++i) {
                    (*this)[i] = this->rng.get() & genosect_mask;
                }
            }

            //! Also need a floating point rng
            morph::RandUniform<float> frng;

            //! Overload the stream output operator
            friend std::ostream& operator<< <N, K> (std::ostream& os, const Genome<N, K>& v);
        };

        template <size_t N=5, size_t K=5>
        std::ostream& operator<< (std::ostream& os, const Genome<N, K>& g)
        {
            os << g.str();
            return os;
        }
    } // bn
} // morph
