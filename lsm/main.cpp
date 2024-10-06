#include "lsm.h"

int main() {
    TLSMTree<int, int> tree("/example/...");

    for (int i = 0; i < 30; ++i) {
        tree.Insert(i, i * 10);
    }
}