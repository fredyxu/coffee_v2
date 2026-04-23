#include "tests.h"

#include "core/utils/log.h"
#include "tests/encoder/test_encoder.h"
#include "tests/state/test_state_contract.h"

void tests(void)
{
    LOG("==== TESTS START ====");

    test_state_contract_run();
    test_encoder_run();

    LOG("==== TESTS DONE ====");
}
