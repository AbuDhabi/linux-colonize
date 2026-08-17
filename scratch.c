#include <stdio.h>
#include "src/core/colony_production.h"
#include "src/core/colony.h"

int main() {
    printf("sol_bonus: 2\n");
    printf("profession: %d (FREE_COLONIST)\n", COLONIZE_PROF_FREE_COLONIST);
    printf("colony_prod_hammers_worker: %d\n", colony_prod_hammers_worker("Lumber Mill", COLONIZE_PROF_FREE_COLONIST, 2, 1));
    return 0;
}
