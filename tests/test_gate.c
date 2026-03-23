// test_gate.c
// Logic gate truth table verification: NOT, AND, OR, NAND, NOR, XOR, XNOR

#include <stdio.h>
#include "../includes/gate.h"
#include "common_test.h"

// ======================== Tests ========================

static int test_not(void) {
    printf("\n=== test_not ===\n");
    ASSERT_EQ_BIT("NOT(0)==1", NOT(0), 1);
    ASSERT_EQ_BIT("NOT(1)==0", NOT(1), 0);
    return 0;
}

static int test_and(void) {
    printf("\n=== test_and ===\n");
    ASSERT_EQ_BIT("AND(0,0)==0", AND(0,0), 0);
    ASSERT_EQ_BIT("AND(0,1)==0", AND(0,1), 0);
    ASSERT_EQ_BIT("AND(1,0)==0", AND(1,0), 0);
    ASSERT_EQ_BIT("AND(1,1)==1", AND(1,1), 1);
    return 0;
}

static int test_or(void) {
    printf("\n=== test_or ===\n");
    ASSERT_EQ_BIT("OR(0,0)==0", OR(0,0), 0);
    ASSERT_EQ_BIT("OR(0,1)==1", OR(0,1), 1);
    ASSERT_EQ_BIT("OR(1,0)==1", OR(1,0), 1);
    ASSERT_EQ_BIT("OR(1,1)==1", OR(1,1), 1);
    return 0;
}

static int test_nand(void) {
    printf("\n=== test_nand ===\n");
    ASSERT_EQ_BIT("NAND(0,0)==1", NAND(0,0), 1);
    ASSERT_EQ_BIT("NAND(0,1)==1", NAND(0,1), 1);
    ASSERT_EQ_BIT("NAND(1,0)==1", NAND(1,0), 1);
    ASSERT_EQ_BIT("NAND(1,1)==0", NAND(1,1), 0);
    return 0;
}

static int test_nor(void) {
    printf("\n=== test_nor ===\n");
    ASSERT_EQ_BIT("NOR(0,0)==1", NOR(0,0), 1);
    ASSERT_EQ_BIT("NOR(0,1)==0", NOR(0,1), 0);
    ASSERT_EQ_BIT("NOR(1,0)==0", NOR(1,0), 0);
    ASSERT_EQ_BIT("NOR(1,1)==0", NOR(1,1), 0);
    return 0;
}

static int test_xor(void) {
    printf("\n=== test_xor ===\n");
    ASSERT_EQ_BIT("XOR(0,0)==0", XOR(0,0), 0);
    ASSERT_EQ_BIT("XOR(0,1)==1", XOR(0,1), 1);
    ASSERT_EQ_BIT("XOR(1,0)==1", XOR(1,0), 1);
    ASSERT_EQ_BIT("XOR(1,1)==0", XOR(1,1), 0);
    return 0;
}

static int test_xnor(void) {
    printf("\n=== test_xnor ===\n");
    ASSERT_EQ_BIT("XNOR(0,0)==1", XNOR(0,0), 1);
    ASSERT_EQ_BIT("XNOR(0,1)==0", XNOR(0,1), 0);
    ASSERT_EQ_BIT("XNOR(1,0)==0", XNOR(1,0), 0);
    ASSERT_EQ_BIT("XNOR(1,1)==1", XNOR(1,1), 1);
    return 0;
}

// Compound logic identity: De Morgan's laws
static int test_demorgan_laws(void) {
    printf("\n=== test_demorgan_laws ===\n");
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            char name[64];
            // NOT(AND(a,b)) == OR(NOT(a), NOT(b))
            snprintf(name, sizeof(name), "DeMorgan1(%d,%d)", a, b);
            ASSERT_EQ_BIT(name, NAND(a,b), OR(NOT(a), NOT(b)));

            // NOT(OR(a,b)) == AND(NOT(a), NOT(b))
            snprintf(name, sizeof(name), "DeMorgan2(%d,%d)", a, b);
            ASSERT_EQ_BIT(name, NOR(a,b), AND(NOT(a), NOT(b)));
        }
    }
    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_not);
    RUN_TEST(test_and);
    RUN_TEST(test_or);
    RUN_TEST(test_nand);
    RUN_TEST(test_nor);
    RUN_TEST(test_xor);
    RUN_TEST(test_xnor);
    RUN_TEST(test_demorgan_laws);
    TEST_SUMMARY("Logic Gate Tests");
    return rc;
}
