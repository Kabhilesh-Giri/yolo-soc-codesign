#include "swEmu.h"

/// @brief Interrupt Service Routine for debug
void SWEmu::isr1() {
    // there is a startup intertrupt for no good reason, 
    // ignore it.
    if (sc_time_stamp() < sc_time(1,SC_PS)){
        return;
    }
    cout << "ISR1 called at " << sc_time_stamp() << endl;
}

/// @brief Interrupt Service Routine for debug
void SWEmu::isr2() {
    // there is a startup intertrupt for no good reason, 
    // ignore it.
    if (sc_time_stamp() < sc_time(1,SC_PS)){
        return;
    }
    cout << "ISR2  called at " << sc_time_stamp() << endl;
}

/// @brief Interrupt Service Routine for debug
void SWEmu::isr3() {
    // there is a startup intertrupt for no good reason, 
    // ignore it.
    if (sc_time_stamp() < sc_time(1,SC_PS)){
        return;
    }
    cout << "ISR3  called at " << sc_time_stamp() << endl;
}

/// @brief Interrupt Service Routine for debug
void SWEmu::isr4() {
    // there is a startup intertrupt for no good reason, 
    // ignore it.
    if (sc_time_stamp() < sc_time(1,SC_PS)){
        return;
    }
    cout << "ISR4  called at " << sc_time_stamp() << endl;
}

void SWEmu::thread_process()
{
    // Set All the Vector Registers with Initial Values 

    const int32_t weights[16] = { 0, 0,  0, 1,  0, 0,  0, -1,
                                0, 0,  0, 1,  0, 0,  0, -1 };

    const int32_t threshold = 4000;

    dev_addr_set(0x49000000ULL); 

    // Set Vector A to Weights
    for (uint8_t i = 0; i < 16; i++)
        word_write((0x04 + (4 * i)), weights[i]);

    // Set Vector B to Zeros
    for (uint8_t index = 0x44; index < 0x44; index += 0x04)
        word_write(index, 0);

    // Set Vector C to Zeros
    for (uint8_t index = 0x84; index < 0xC3; index += 0x04)
        word_write(index, 0);

    // Power On the Rx
    dev_addr_set(0x50000000ULL); 

    usleep(10); // simulate some startup  delay

    word_write(0x0, 1);

    // Polling to check the Vector Processor CSR == 5 or not
    dev_addr_set(0x49000000ULL); 

    while(word_read(0x00) == 0)
    {
        pollingCounts++;
    }

    while(word_read(0x00) == 5)
    {
        pollingCounts++;
    }

    printf("CPU pollingCounts : %d\n", pollingCounts);

    // Print the VC 
    for (uint8_t index = 0x84; index < 0xC4; index += 0x08)
    {
        int32_t i_val = static_cast<int32_t>(word_read(index));
        int32_t q_val = static_cast<int32_t>(word_read(index + 0x04));

        printf("CPU : VC_i_val %d, VC_q_val %d\n", i_val, q_val);
    }    

    // --- done ---

    // stop simulation and exit, done with the testbench
    retVal = 0; // all good (set process exit value)
    sc_stop();

}
