#include <morph/bn/Genome.h>
#include <morph/bn/GeneNet.h>

int main()
{
    // Flip probability
    float p = 0.01f;

    morph::bn::Genome<6, 4> g;
    g.randomize();
    std::cout << "Genome 1: " << g << std::endl;
    // Get a copy:
    morph::bn::Genome<6, 4> g2 = g;
    std::cout << "Genome 2 after copy: " << g2 << std::endl;

    g2.evolve (p);
    std::cout << "Genome 2 evolved: " << g2 << std::endl;
    std::cout << "Hamming distance between them: " << g.hamming(g2) << std::endl;

    g.evolve (p);
    std::cout << "Genome 1 evolved: " << g << std::endl;

    return 0;
}
