#include <iostream>
#include <fstream>
#include <thread>
#include <random>
#include <Eigen/Dense>
#include "Retina.h"
#include "tool.h"
#include "GA.h"

// thread_local int TID;

void read_param()
{
    char aux[20];

    std::ifstream f("PARAM");
    if (f.is_open())
    {
        f >> aux >> THREADS >> aux >> ITERS >> aux >> POPULATION >> aux >> ELITES
          >> aux >> T >> aux >> TAU >> aux >> DT >> aux >> ETA >> aux >> EPOCHS
          >> aux >> CELLS >> aux >> RGCS
          >> aux >> NOISE >> aux >> TRAIN_SIZE >> aux >> TEST_SIZE
          >> aux >> W_COST(LOSS) >> aux >> W_COST(AUC) >> aux >> W_COST(N_SYNAPSES);

        f.close();

        if (W_COST.sum() != 1) std::invalid_argument("W_COST");
    } else std::cout << "PARAM not found." << std::endl;
}

void write(Genome *g, Retina *r, const int tid)
{
    (void) std::system("mkdir -p results/");

    for (int i = 0; i < ELITES; i++)
    {
        std::string nameg = "results/" + std::to_string(tid) + "_"
                            + std::to_string(i) + "g.txt";
        std::ofstream fg(nameg);
        fg << g[i] << std::endl;
        fg.close();

        std::string namer = "results/" + std::to_string(tid) + "_"
                            + std::to_string(i) + "r.txt";
        std::ofstream fr(namer);
        fr << r[i] << std::endl;
        fr.close();
    }
}

void fork(int tid)
{
    MatrixXd sigs, st;

    generate(sigs, st, TRAIN_SIZE + TEST_SIZE, 1);

    Genome g[POPULATION];
    Retina r[POPULATION];

    for (int i = 0; i < POPULATION; i++) r[i].init(g[i]);

    GA sim = GA(g, r);
    sim.run(sigs, st, tid);

    write(g, r, tid);
}

int main()
{
    read_param();

    // TODO: Multi-thread
    std::thread ths[THREADS];
    for (int i = 0; i < THREADS; i++) ths[i] = std::thread(fork, i);
    for (int i = 0; i < THREADS; i++) ths[i].join();

    return 0;
}
