#include <stdio.h>
#include <stdint.h>
#include "src/core/colony.h"

int main() {
    FILE* f = fopen("tests/golden/test-saves-golden/COLONY00.SAV", "rb");
    if (!f) return 1;
    
    // Seek to Fort Orange (offset in savefile?) Or just use the test!
    return 0;
}
