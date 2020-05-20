#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
#include <Eigen/Dense>
#include "Retina.h"
#include "tool.h"
#include "GA.h"
using Eigen::MatrixXd;

GA::GA(Genome *genomes, Retina *retinas)
{
    g = genomes;
    r = retinas;
    children = new Genome[POPULATION - ELITES];
    p1 = new int[POPULATION];
    p2 = new int[POPULATION];
}

void GA::eval(const MatrixXd &x, const MatrixXd &y)
{
    for (int i = 0; i < POPULATION; i++)
    {
        MatrixXd retina_out;
        r[i].react(x, retina_out);

        double auc = 0;
        g[i].costs(LOSS) = model(&auc, retina_out, y);
        g[i].costs(AUC) = auc;
        g[i].total_cost = W_COST.dot(g[i].costs);
    }
}


int comparator(const void *r1, const void *r2)
{
    /*
     * If r1's cost > r2's cost (r1 is woge), r1 ranks lower than r2. Return 1.
     * If r1's cost < r2's cost (r2 is woge), r1 ranks higher than r2. Return -1.
     * Else, return 0.
     */
    const Genome *r_a = (Genome *) r1;
    const Genome *r_b = (Genome *) r2;

    double a = r_a->total_cost, b = r_b->total_cost;

    if (a != a && b != b) return 0;
    else if (a != a) return 1;
    else if (b != b) return -1;

    if (a > b)      return 1;
    else if (a < b) return -1;
    else return 0;
}

int GA::select_p(const int cur, const int p_)
{
    int p, rival1, rival2;
    do
    {// Randomly select two rivals
        rival1 = rand() % POPULATION;
        rival2 = rand() % POPULATION;
    } while (rival1 == cur || rival2 == cur || rival1 == p_ || rival2 == p_);

    double ratio = g[rival1].total_cost / g[rival2].total_cost;
    if (ratio > 1) ratio = 1.0 / ratio; // Assume the range is (0, 1]

    if (rand() % 100 < 100 - 50 * ratio) // Pick the one with the lower cost
        p = (g[rival1].total_cost < g[rival2].total_cost)? rival1 : rival2;
    else // Pick the one with the high cost (loser)
        p = (g[rival1].total_cost < g[rival2].total_cost)? rival2 : rival1;

    return p;
}

void GA::selection()
{
    for (int i = 0; i < POPULATION - ELITES; i++)
    { // Choose each child's parents
        p1[i] = select_p(i + ELITES, -1);
        p2[i] = select_p(i + ELITES, p1[i]);
    }
}

void GA::crossover()
{
    for (int i = 0; i < POPULATION - ELITES; i++)
    {
        int p, q;

        if (rand() % 100 < 50)  p = p1[i];
        else                    p = p2[i];

        int n = g[p].n_types;

        for (int j = 0; j < n; j++)
        {
            if (rand() % 100 < 50)  p = p1[i];
            else                    p = p2[i];

            if (j == 0)          q = 0; // Receptor cells
            else if (j == n - 1) q = g[p].n_types - 1; // Ganglion cells
            else
            { // Interneurons
                if (g[p].n_types > 2) // Has interneurons
                    q = 1 + rand() % (g[p].n_types - 2);
                else
                { // Another parent must have interneurons
                    p = p == p1[i] ? p2[i] : p1[i];
                    q = 1 + rand() % (g[p].n_types - 2);
                }
            }

            children[i].axon[j] = g[p].axon[q];
            children[i].dendrite[j] = g[p].dendrite[q];
            children[i].polarity[j] = g[p].polarity[q];
            children[i].n_cell[j] = g[p].n_cell[q];
            children[i].phi[j] = g[p].phi[q];
            children[i].beta[j] = g[p].beta[q];
        }
        children[i].n_types = n;
    }

    for (int i = 0; i < POPULATION - ELITES; i++)
    {
        g[ELITES + i].n_types = children[i].n_types;

        for (int j = 0; j < MAX_TYPES; j++)
        {
            g[ELITES + i].axon[j] = children[i].axon[j];
            g[ELITES + i].dendrite[j] = children[i].dendrite[j];
            g[ELITES + i].polarity[j] = children[i].polarity[j];
            g[ELITES + i].n_cell[j] = children[i].n_cell[j];
            g[ELITES + i].phi[j] = children[i].phi[j];
            g[ELITES + i].beta[j] = children[i].beta[j];
        }
    }
}

void GA::mutation()
{
    for (int i = ELITES; i < POPULATION; i++)
    {
        int n = g[i].n_types;

        // Randomly flip two bits for each axon and dendrite descriptor
        for (int j = 0; j < n; j++)
        {
            for (int a = 0; a < 2; a++)
            {
                int k1, k2;
                uniform(k1, 0, 32);
                uniform(k2, 0, 32);
                g[i].axon[j][k1] = ~g[i].axon[j][k1];
                g[i].dendrite[j][k2] = ~g[i].dendrite[j][k2];
            }
        }

        for (int j = 0; j < n; j++)
        {
            // Mutate polarity, in very small amount per time
            logitnormal(g[i].polarity[j], 0.005, -1, 1);

            // Mutate phi, in very small amount per time
            logitnormal(g[i].phi[j], 0.01, 0.1, 10);

            // Mutate beta, in very small amount per time
            logitnormal(g[i].beta[j], 0.1, -1, 1);

            // Skip receptog and ganglion cells
            if (j == 0) continue;

            int chance = rand() % 100;
            // 0.05 probability the number of cells will decrease/increase by 1
            if (chance < 5)
            {
                if (++g[i].n_cell[j] > CELLS) g[i].n_cell[j] = CELLS;
            } else if (chance < 10)
            {
                int min_cell = (j == n - 1)? 2 : 0;
                if (--g[i].n_cell[j] < min_cell) g[i].n_cell[j] = min_cell;
            }
        }

        // Force the receptog to have excitatory projections
        // if (g[i].polarity[0] < 0)
        //     g[i].polarity[0] = fabs(g[i].polarity[0]);
    }
}

void GA::run(const MatrixXd &x, const MatrixXd &y, const int tid = 0)
{
    // Open a log
    std::ofstream f("results/log.txt");

    for (int i = 0; i < ITERS; i++)
    {
        std::cout << "\n[" << tid << "]" << i + 1;
        std::cout.flush();

        for (int j = 0; j < POPULATION; j++)
        {
            g[j].organize();
            r[j].init(g[j]);
        }

        eval(x, y);

        // Sort the retinas
        qsort(g, POPULATION, sizeof(Genome), comparator);

        // Output stats
        for (int j = 0; j < POPULATION; j++) f << g[j].total_cost << "," << g[j].costs;
        f << "\n";

        selection();
        crossover();
        mutation();
	}

    // eval and sort the final retinas
    eval(x, y);
    qsort(g, POPULATION, sizeof(Retina), comparator);

	f.close();

    delete[] children;
    delete[] p1;
    delete[] p2;
}
