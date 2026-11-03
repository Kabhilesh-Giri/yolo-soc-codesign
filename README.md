## Overview 

The aim of this assignment is to build a *vector operation accelerator* in SystemC and 
drive the accelerator from SW on an ARM. To reach these steps, you will first learn 
how to cross compile for and simulate an ARM processor. You will then inspect a 
ready-made demo for a simple debug device (courtesy of Xilinx's [Xilinx CoSimulation Demo](https://github.com/Xilinx/systemctlm-cosim-demo)). After that you'll gradually add features to the debug device and morph into a full fledged vector operation accelerator. 

The instructions for ths lab are detailed in the following steps:

 1. (Reserved for feedback branch pull request. You will receive top level feedback there). 
 2. [Getting Started](.github/STARTING_ISSUES/2.%20Getting%20Started.md)
 3. [Add CSR Register](.github/STARTING_ISSUES/3.%20Add%20CSR%20Register.md)
 4. [Simulate Processing Delay](.github/STARTING_ISSUES/4.%20Simulate%20Processing%20Delay.md)
 5. [Implement Vector Adder](.github/STARTING_ISSUES/5.%20Implement%20Vector%20Adder.md)
 6. [Additional Vector Instructions](.github/STARTING_ISSUES/6.%20Additional%20Vector%20Instructions.md)

***Keep an eye out for question inside issues. Your answers to these questions need to be posted as comments on the questions's respective issue. Your answers to these questions will be graded so please don't overlook them***

After accepting this assignment in github classroom, each step is converted into a [github issue](https://docs.github.com/en/issues). Follow the issues in numerically increasing issue number (the first issue is typically on the bottom of the list). 

## File Structure

Name | Content 
-----|--------
sw | Software code for reading from Debug Device 
hw  | Hardware code for emulating debug device in QEMU co-simulation. And Testbench for stand alone simulation with SWEmu to validate interaction without QEMU. 
libsystemctlm-soc | cosimulation library

The `hw` directory contains in addition to the zynq_demo for cosimulation, a `testbench` for host compiled simulation of SW (in class `SWEmu`).

## Vector Operation Accelerator Register Map

At the end of this lab the full vector operation accelerator's register map should be:

| **Address Space Offset** | **Name**  | **Description**                                                                                                                                                      |
|----------------------|-------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0x00                 | CSR   | The control and status register of the vector adder. The CSR is responsible for 1) Initiating vector operations and 2) Defining the type of operation performed  |
| 0x04                 | VA | The base address of the first vector operand                                                                                                                     |
| 0x44                 | VB | The base address of the second vector operand                                                                                                                    |
| 0x84                 | VC | The base address of the output vector                                                                                                                            |
| 0xC4                 | TRACE | The address of the trace register used for determining the current SystemC simulation time|

## Vector Accelerator Opcodes

The table lists the operations that will be implemented by the vector processor by the end of the lab. Each operation performs element wise operations on the vectors. The type of operation performed depends on the value written in the CSR register as pertable below, which also shows each instruction's opcode and mnemonic.

| **Instruction**         |                   | **MNEMONIC** | **CSR Op Code** | 
|-------------------------|-------------------|--------------|-----------------|
| Add                     | VC = VA + VB      | ADD          | 0x1             |
| Subtract                | VC = VA - VB      | SUB          | 0x2             |
| Multiply                | VC = VA * VB      | MUL          | 0x3             |
| Divide                  | VC = VA / VB      | DIV          | 0x4             |
| Multiply AND Accumulate | VC = VA * VB + VC | MAC          | 0x5             |



## General Rules


Please commit your code frequently or at e very logical break. Each commit should have a meaningful commit message and [cross reference](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/autolinked-references-and-urls#issues-and-pull-requests) the issue the commit belongs to. Always reference the relevant issue(s) in your commits. 

Please comment on each issue with the problems faced and your approach to solve them. Close an issue when done. 


