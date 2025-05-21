#include "sim_pipe.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>

//#define DEBUG

using namespace std;

//used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {"PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP"};

/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */


/* converts integer into array of unsigned char - little indian */
inline void int2char(unsigned value, unsigned char *buffer){
	memcpy(buffer, &value, sizeof value);
}

/* converts array of char into integer - little indian */
inline unsigned char2int(unsigned char *buffer){
	unsigned d;
	memcpy(&d, buffer, sizeof d);
	return d;
}

/* implements the ALU operations */
unsigned alu(opcode_t opcode, unsigned a, unsigned b, unsigned imm, unsigned npc){
	switch(opcode){
			case ADD:
				return (a+b);
			case ADDI:
				return(a+imm);
			case SUB:
				return(a-b);
			case SUBI:
				return(a-imm);
			case XOR:
				return(a ^ b);
			case LW:
			case SW:
				return(a + imm);
			case BEQZ:
			case BNEZ:
			case BGTZ:
			case BGEZ:
			case BLTZ:
			case BLEZ:
			case JUMP:
				return(npc+imm);
			default:	
				return (-1);
	}
}

/* returns true if the instruction is a taken branch/jump */
bool taken_branch(opcode_t opcode, unsigned a){
        switch(opcode){
                case BEQZ:
                        if (a==0) return true;
                        break;
                case BNEZ:
                        if (a!=0) return true;
                        break;
                case BGTZ:
                        if ((int)a>0)  return true;
                        break;
                case BGEZ:
                        if ((int)a>=0) return true;
                        break;
                case BLTZ:
                        if ((int)a<0)  return true;
                        break;
                case BLEZ:
                        if ((int)a<=0) return true;
                        break;
                case JUMP:
                        return true;
                default:
                        return false;
        }
        return false;
}

/* return the kind of instruction encoded */ 

bool is_branch(opcode_t opcode){
        return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ || opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode){
        return (opcode == LW || opcode == SW);
}

bool is_int_r(opcode_t opcode){
        return (opcode == ADD || opcode == SUB || opcode == XOR);
}

bool is_int_imm(opcode_t opcode){
        return (opcode == ADDI || opcode == SUBI);
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* loads the assembly program in file "filename" in instruction memory at the specified address */
void sim_pipe::load_program(const char *filename, unsigned base_address){

   /* initializing the base instruction address */
   instr_base_address = base_address;

   /* creating a map with the valid opcodes and with the valid labels */
   map<string, opcode_t> opcodes; //for opcodes
   map<string, unsigned> labels;  //for branches
   for (int i=0; i<NUM_OPCODES; i++)
	 opcodes[string(instr_names[i])]=(opcode_t)i;

   /* opening the assembly file */
   ifstream fin(filename, ios::in | ios::binary);
   if (!fin.is_open()) {
      cerr << "error: open file " << filename << " failed!" << endl;
      exit(-1);
   }

   /* parsing the assembly file line by line */
   string line;
   unsigned instruction_nr = 0;
   while (getline(fin,line)){
	// set the instruction field
	char *str = const_cast<char*>(line.c_str());

  	// tokenize the instruction
	char *token = strtok (str," \t");
	map<string, opcode_t>::iterator search = opcodes.find(token);
        if (search == opcodes.end()){
		// this is a label for a branch - extract it and save it in the labels map
		string label = string(token).substr(0, string(token).length() - 1);
		labels[label]=instruction_nr;
                // move to next token, which must be the instruction opcode
		token = strtok (NULL, " \t");
		search = opcodes.find(token);
		if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
	}
	instr_memory[instruction_nr].opcode = search->second;

	//reading remaining parameters
	char *par1;
	char *par2;
	char *par3;
	switch(instr_memory[instruction_nr].opcode){
		case ADD:
		case SUB:
		case XOR:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "R"));
			break;
		case ADDI:
		case SUBI:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].immediate = strtoul (par3, NULL, 0); 
			break;
		case LW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src2 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].label = par2;
			break;
		case JUMP:
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].label = par2;
		default:
			break;

	} 

	/* increment instruction number before moving to next line */
	instruction_nr++;
   }
   //reconstructing the labels of the branch operations
   int i = 0;
   while(true){
   	instruction_t instr = instr_memory[i];
	if (instr.opcode == EOP) break;
	if (instr.opcode == BLTZ || instr.opcode == BNEZ ||
            instr.opcode == BGTZ || instr.opcode == BEQZ ||
            instr.opcode == BGEZ || instr.opcode == BLEZ ||
            instr.opcode == JUMP
	 ){
		instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
	}
        i++;
   }

}

/* writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness) */
void sim_pipe::write_memory(unsigned address, unsigned value){
	int2char(value,data_memory+address);
}

/* prints the content of the data memory within the specified address range */
void sim_pipe::print_memory(unsigned start_address, unsigned end_address){
	cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": "; 
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3) cout << endl;
	} 
}

/* prints the values of the registers */
void sim_pipe::print_registers(){
        cout << "Special purpose registers:" << endl;
        unsigned i, s;
        for (s=0; s<NUM_STAGES; s++){
                cout << "Stage: " << stage_names[s] << endl;
                for (i=0; i< NUM_SP_REGISTERS; i++)
                        if ((sp_register_t)i != IR && (sp_register_t)i != COND && get_sp_register((sp_register_t)i, (stage_t)s)!=UNDEFINED) cout << reg_names[i] << " = " << dec <<  get_sp_register((sp_register_t)i, (stage_t)s) << hex << " / 0x" << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
        }
        cout << "General purpose registers:" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++)
                if (get_gp_register(i)!=(int)UNDEFINED) cout << "R" << dec << i << " = " << get_gp_register(i) << hex << " / 0x" << get_gp_register(i) << endl;
}

/* initializes the pipeline simulator */
sim_pipe::sim_pipe(unsigned mem_size, unsigned mem_latency){
	data_memory_size = mem_size;
	data_memory_latency = mem_latency;
	data_memory = new unsigned char[data_memory_size];
	reset();
}
	
/* deallocates the pipeline simulator */
sim_pipe::~sim_pipe(){
	delete [] data_memory;
	//delete [] instr_ptr;
}


/* execution statistics */
unsigned sim_pipe::get_clock_cycles(){return clock_cycles;}

unsigned sim_pipe::get_instructions_executed(){return instructions_executed;}

unsigned sim_pipe::get_stalls(){return stalls;}

float sim_pipe::get_IPC(){return (float)instructions_executed/clock_cycles;}
                                
/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */


/* reset the state of the pipeline simulator */
void sim_pipe::reset(){

	// initializing data memory to all 0xFF
	for (unsigned i=0; i<data_memory_size; i++) data_memory[i]=0xFF;

	// initializing instuction memory
        for (int i=0; i<PROGRAM_SIZE;i++){
                instr_memory[i].opcode=(opcode_t)NOP;
                instr_memory[i].src1=UNDEFINED;
                instr_memory[i].src2=UNDEFINED;
                instr_memory[i].dest=UNDEFINED;
                instr_memory[i].immediate=UNDEFINED;
        }
	instr_base_address = UNDEFINED;

	// general purpose registers initialization
	for (int i=0; i<NUM_GP_REGISTERS;i++)
	{
		gp_registers[i]=UNDEFINED;
	}


	for (int i=0; i < NUM_SP_REGISTERS; i++)
	{
		for (int j=0; j < NUM_STAGES; j++)
			{
				sp_registers[i][j]=UNDEFINED;
			}
	}

	// IR initialization
	for (int i=0; i<NUM_STAGES-1; i++){
		ir[i].opcode=(opcode_t)NOP;
		ir[i].src1=UNDEFINED;
		ir[i].src2=UNDEFINED;
		ir[i].dest=UNDEFINED;
		ir[i].immediate=UNDEFINED;
	}

	// other required initializations (statistics, etc.)
	clock_cycles = 0; //clock cycles
	stalls = 0; //stalls
	cstalls=0;
	instructions_executed = 0; //instruction count

	//control bit initialization
	
		raw_hazard=0;
		raw_hazard_propagate=0;
		raw_hazard_propagate_2=0;
		
		pc_temp=UNDEFINED;

		control_hazard=0;
		control_hazard_propagate=0;
		control_hazard_propagate_2=0;
		control_hazard_propagate_3=0;
		
		structural_mem_hazard=0;
	//	structural_mem_hazard_propagate=0;
	//	structural_mem_hazard_propagate_2=0;
	//	structural_mem_hazard_propagate_3=0;
		mem_hazard_pipe_freeze=0;
		latency_tracker=0;
}

//returns value of special purpose register (see sim_pipe.h for more details)
unsigned sim_pipe::get_sp_register(sp_register_t reg, stage_t s)
{
	if (reg==PC && s==IF && clock_cycles==0)
	{
		return instr_base_address;  //before simulator is run return instruction base address
	}

	if(reg==PC && s==IF && structural_mem_hazard==1 && raw_hazard==0)
	{
		return pc_temp;
	}

	else
	{
	return sp_registers[reg][s];  // return special purpose register and stage
	}
}

//returns value of general purpose register
int sim_pipe::get_gp_register(unsigned reg)
{
	return gp_registers[reg];
}

//sets the value of referenced general purpose register
void sim_pipe::set_gp_register(unsigned reg, int value)
{
	 
	
       gp_registers[reg]=value;
	
}

/* <TODO: BODY OF THE SIMULATOR */
// Note: processing the stages in reverse order simplifies the data propagation through pipeline registers
void sim_pipe::run(unsigned cycles){

	unsigned start_cycles = clock_cycles;
	/* initialization at the beginning of simulation */
	if (clock_cycles == 0)
	{
		// <set PC register to instr_base_address>
		sp_registers[PC][IF]=instr_base_address;

	}

	/* ====== MAIN SIMULATION LOOP (one iteration per clock cycle)  ========= */
	while(cycles==0 || clock_cycles-start_cycles!=cycles){

                /* =============== */
                /* PIPELINE STAGES */
                /* =============== */

		/* ============   WB stage   ============  */
		
		


			// <hint: the simulation loop should be exited when the instruction processed is EOP>
	
		if(is_int_r(ir[MEM].opcode))
		{
			set_gp_register(ir[MEM].dest, sp_registers[ALU_OUTPUT][WB]);

			instructions_executed++;
		}

		if(is_int_imm(ir[MEM].opcode))
		{
			set_gp_register(ir[MEM].dest, sp_registers[ALU_OUTPUT][WB]);
			instructions_executed++;
		}

		if(is_memory(ir[MEM].opcode))
		{
			if(ir[MEM].opcode==LW)
			{
				if(sp_registers[LMD][WB]>127)
				{

					set_gp_register(ir[MEM].dest , (sp_registers[LMD][WB]-256));
				}
				if(sp_registers[LMD][WB]<=127)
				{

					set_gp_register(ir[MEM].dest , sp_registers[LMD][WB]);
				}

			}
			instructions_executed++;
		}

		if(is_branch(ir[MEM].opcode))
		{
			instructions_executed++;
		}

		if(ir[MEM].opcode == EOP)
		{
			break;
		}

		
	
//		cout<< " Instruction at the end of WB stage: "<< ir[MEM].opcode<< " Destination register : " << ir[MEM].dest << " Source 1: " << ir[MEM].src1 << " Source 2: " << ir[MEM].src2 << " Immediate :" << ir[MEM].immediate<<" ALU output WB : " << dec << sp_registers[ALU_OUTPUT][WB] << " LMD WB: " << dec << sp_registers[LMD][WB]<< endl;
		/* ============   MEM stage   ===========  */

		if(is_int_r(ir[EXE].opcode))
		{
			sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
			ir[MEM]=ir[EXE];
			sp_registers[LMD][WB]=UNDEFINED;

		}

		if(is_int_imm(ir[EXE].opcode))
		{
			sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
			ir[MEM]=ir[EXE];
			sp_registers[LMD][WB]=UNDEFINED;
		}


		if(is_memory(ir[EXE].opcode))
		{
			//ir[MEM]=ir[EXE];
			if(structural_mem_hazard==0)
			{
				//struct_mem_hazard=0;
				ir[MEM]=ir[EXE];
				if(ir[EXE].opcode==SW)
				{
					write_memory(sp_registers[ALU_OUTPUT][MEM], sp_registers[B][MEM]);
					sp_registers[LMD][WB]=UNDEFINED;
					sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
				}
				if(ir[EXE].opcode==LW)
				{
					sp_registers[LMD][WB]=data_memory[sp_registers[ALU_OUTPUT][MEM]];
					sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
				}
			}
			if(structural_mem_hazard==1)
			{
				latency_tracker++;
				if(latency_tracker <= data_memory_latency)
				{
					ir[MEM].opcode=NOP;
					sp_registers[ALU_OUTPUT][WB]=UNDEFINED;
					sp_registers[LMD][WB]=UNDEFINED;
					//latency_tracker++;
				//	cout << " Latency_tracker < data memory latency " << endl;
				//	cout << " Latency_tracker " << latency_tracker << endl;
				//	cout << " Data memory Latency " <<  data_memory_latency << endl;
					stalls++;
				}
				if(latency_tracker>data_memory_latency)
				{
					mem_hazard_pipe_freeze=0;
					latency_tracker=0;
					structural_mem_hazard=0;
					ir[MEM]=ir[EXE];
				//	cout << " Latency tracker >= data memory latency " << endl;
					if(ir[EXE].opcode==SW)
					{
						write_memory(sp_registers[ALU_OUTPUT][MEM], sp_registers[B][MEM]);
						sp_registers[LMD][WB]=UNDEFINED;
						sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
					}
					if(ir[EXE].opcode==LW)
					{
						sp_registers[LMD][WB]=data_memory[sp_registers[ALU_OUTPUT][MEM]];
						sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
					}
				}
				//latency_tracker++;
			}


		}


		if (is_branch(ir[EXE].opcode))
		{
			control_hazard=0;
			ir[MEM]=ir[EXE];
			if(sp_registers[COND][MEM]==0)
			{
				sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
			}
			else
			{
				sp_registers[ALU_OUTPUT][WB]=sp_registers[PC][IF]+8;
			}
			sp_registers[LMD][WB]=UNDEFINED;
			//sp_registers[ALU_OUTPUT][WB]=sp_registers[ALU_OUTPUT][MEM];
			//cout << " sp_registers alu wb in brnach EXE "<< sp_registers[ALU_OUTPUT][WB] << endl;
			sp_registers[COND][WB]=sp_registers[COND][MEM];
		}

		if(ir[EXE].opcode==EOP)
		{
			ir[MEM]=ir[EXE];

			sp_registers[ALU_OUTPUT][WB]=UNDEFINED;
			sp_registers[LMD][WB]=UNDEFINED;
		}

		if((ir[EXE].opcode==NOP)  && raw_hazard_propagate_2==1)
			{
				raw_hazard=0;   //clear data hazard as previous instruction has already updated register state
				raw_hazard_propagate_2=0;
					
			//	cout << " Data Hazard in MEM stage: " << data_hazard_1 << endl;

				/*re-initialize MEM*/
				ir[MEM].opcode=NOP;
				ir[MEM].src1=UNDEFINED;
				ir[MEM].src2=UNDEFINED;
				ir[MEM].dest=UNDEFINED;
				ir[MEM].immediate=UNDEFINED;

				/*bookkeeping*/
				sp_registers[ALU_OUTPUT][WB]=UNDEFINED;
				sp_registers[LMD][WB]=UNDEFINED;
			}

		if(ir[EXE].opcode==NOP && control_hazard_propagate_3==1)
		{
	
			ir[MEM]=ir[EXE];
			control_hazard_propagate_3=0;
			sp_registers[ALU_OUTPUT][WB]=UNDEFINED;
			sp_registers[COND][WB]=UNDEFINED;
			sp_registers[LMD][WB]=UNDEFINED;
		}

	

//		cout<< " Instruction at the end of MEM (ir[MEM]) stage opcode=> "  << ir[MEM].opcode<< " Source 1: " << ir[MEM].src1 << " Source 2: " << ir[MEM].src2 << " Destination: " << ir[MEM].dest << " Immediate: " <<ir[MEM].immediate <<" ALU OUTPUT WB: "<< dec << sp_registers[ALU_OUTPUT][WB] << " LMD WB: " << dec<< sp_registers[LMD][WB]<< endl;

		/* ============   EXE stage   ===========  */
		if(mem_hazard_pipe_freeze==0)
		{
//			cout << " EXE stage running " << endl;
			if(is_int_r(ir[ID].opcode))
			{
				sp_registers[ALU_OUTPUT][MEM]=alu(ir[ID].opcode, sp_registers[A][EXE], sp_registers[B][EXE], sp_registers[IMM][EXE], sp_registers[NPC][EXE]);
				ir[EXE]= ir[ID];
				sp_registers[B][MEM]=sp_registers[B][EXE];
				sp_registers[COND][MEM]=UNDEFINED;
			}


			if (is_int_imm(ir[ID].opcode))
			{
				sp_registers[ALU_OUTPUT][MEM]=alu(ir[ID].opcode, sp_registers[A][EXE], sp_registers[B][EXE], sp_registers[IMM][EXE], sp_registers[NPC][EXE]);
				ir[EXE]=ir[ID];
				sp_registers[B][MEM]=sp_registers[B][EXE];
				sp_registers[COND][MEM]=UNDEFINED;
			}


			if (is_memory(ir[ID].opcode))
			{
				if (data_memory_latency>0)
				{structural_mem_hazard=1;}
//				cout << " in memory ir[ID].opcode check and assign to ir[EXE] " << endl;
				sp_registers[ALU_OUTPUT][MEM]=alu(ir[ID].opcode, sp_registers[A][EXE], sp_registers[B][EXE], sp_registers[IMM][EXE], sp_registers[NPC][EXE]);
				ir[EXE]=ir[ID];
				sp_registers[B][MEM]=sp_registers[B][EXE];
				sp_registers[COND][MEM]=UNDEFINED;
			}
	

			if(is_branch(ir[ID].opcode))
			{
			//control_hazard=0;
		
			//cout << " EXE stage Branch code use ID.opcode " << endl;

				if (taken_branch(ir[ID].opcode, sp_registers[A][EXE]))
				{
				sp_registers[COND][MEM]=0;
				ir[EXE]=ir[ID];
				sp_registers[B][MEM]=UNDEFINED;
			//	cout << " Branch Taken " <<endl;	
				sp_registers[ALU_OUTPUT][MEM]=alu(ir[ID].opcode, sp_registers[A][EXE], sp_registers[B][EXE],sp_registers[IMM][EXE], sp_registers[NPC][EXE]);
		//		cout << " ALU_OUTPUT MEM " << sp_registers[ALU_OUTPUT][MEM]<<endl;
				}
				else
				{
					sp_registers[ALU_OUTPUT][MEM]=sp_registers[PC][IF]+8;
					sp_registers[COND][MEM]=UNDEFINED;
					ir[EXE]=ir[ID];
					sp_registers[B][MEM]=UNDEFINED;
			//	cout << " Branch not taken "<< endl;
				}
			}

			if(ir[ID].opcode==EOP)
			{
				ir[EXE]=ir[ID];

				sp_registers[ALU_OUTPUT][MEM] = UNDEFINED;

				sp_registers[COND][MEM] = UNDEFINED;
				sp_registers[B][MEM]=UNDEFINED;

			}
		
			if(ir[ID].opcode==NOP && raw_hazard_propagate==1)
			{
		
				ir[EXE].opcode=NOP;
				ir[EXE].src1=UNDEFINED;
				ir[EXE].src2=UNDEFINED;
				ir[EXE].dest=UNDEFINED;
				ir[EXE].immediate=UNDEFINED;
				
				raw_hazard=0;
				raw_hazard_propagate=0;
				raw_hazard_propagate_2=1;
				
				sp_registers[COND][MEM]=UNDEFINED;
				sp_registers[ALU_OUTPUT][MEM]=UNDEFINED;
				sp_registers[B][MEM]=UNDEFINED;
				
			
			}

			if(ir[ID].opcode==NOP && control_hazard_propagate_2==1)
			{
				ir[EXE]=ir[ID];
			//cout << " EXE stage control hazard propagate using ID.opcode " << endl;
				control_hazard_propagate_3=1;
				control_hazard_propagate_2=0;
				sp_registers[COND][MEM]=UNDEFINED;
				sp_registers[ALU_OUTPUT][MEM]=UNDEFINED;
				sp_registers[B][MEM]=UNDEFINED;
			}



		}

//	cout<< " Instruction at the end of EXE stage(ir[EXE) opcode=> "  << ir[EXE].opcode <<" Source 1(ir[EXE].src1: " << ir[EXE].src1 << " Source 2: " <<ir[EXE].src2 << " Destination: " << ir[EXE].dest << " Immediate(ir[EXE].immediate): " << ir[EXE].immediate << " ALU OUTPUT MEM : "<<dec << sp_registers[ALU_OUTPUT][MEM] << " B MEM : " <<dec <<sp_registers[B][MEM]<< endl;
			// <suggestion: use "alu" and "taken_branch" helper functions above to update ALU_OUTPUT and COND registers>

		/* ============   ID stage   ============  */
		if(mem_hazard_pipe_freeze==0)
		{
//			cout << " ID stage running " << endl;
			if(is_int_r(ir[IF].opcode))
			{
				if(ir[IF].src1==ir[EXE].dest || ir[IF].src2==ir[EXE].dest ) // check for data hazard on both source 1 and 2 registers
				{
					raw_hazard=1;
					stalls++;
				}

				if(ir[IF].src1==ir[MEM].dest || ir[IF].src2 == ir[MEM].dest)
				{
					raw_hazard=1;
					stalls++;
				}

				if (raw_hazard==0)               // if no data hazard, pass instruction through
				{
					
					/* Pass through SRC1 register value to A, SRC2 register value to B and NPC to the next pipeline register stage. Immediate field is not used*/
					sp_registers[A][EXE]=get_gp_register(ir[IF].src1);
					sp_registers[B][EXE]=get_gp_register(ir[IF].src2);
					sp_registers[NPC][EXE]= sp_registers[NPC][ID];
					sp_registers[IMM][EXE]=UNDEFINED;
					ir[ID] = ir[IF];
				}
				if(raw_hazard==1)				// if data hazard present, pass NOP and set pipe registers to undefined	
				{
					ir[ID].opcode=NOP;
					raw_hazard_propagate=1;
					sp_registers[A][EXE]=UNDEFINED;
					sp_registers[B][EXE]=UNDEFINED;
					sp_registers[NPC][EXE]=UNDEFINED;
					sp_registers[IMM][EXE]=UNDEFINED;
				}

			}

			if(is_int_imm(ir[IF].opcode))
			{
				if(ir[IF].src1 == ir[EXE].dest)
				{
//					cout << " Data Hazard 1 check , is_int_imm " << endl;
					raw_hazard=1;
//					cout << " Data Hazard 1 in imm " << raw_hazard  << endl;
					stalls++;

				}
				if(ir[IF].src1 == ir[MEM].dest)
				{
					raw_hazard=1;
					stalls++;
//					cout << " Data Hazard 2 in imm" << raw_hazard << endl;
				}

				if (raw_hazard ==0)
				{

					sp_registers[A][EXE]=get_gp_register(ir[IF].src1);

				/*sign extend immediate field depending on MSB*/

					if(/*(0x8000 & ir[IF].immediate) == 0x0000*/ ir[IF].immediate >= 0) //if MSB is 0
					{
						sp_registers[IMM][EXE]=0x00000000 |ir[IF].immediate; //extend MSB
					}
					if (/*(0x8000 & ir[IF].immediate) == 0x8000*/ir[IF].immediate < 0) 
					{
						sp_registers[IMM][EXE]=0xFFFF0000 | ir[IF].immediate; //extend MSB
					}
					sp_registers[NPC][EXE]=sp_registers[NPC][ID];
					sp_registers[B][EXE]=UNDEFINED;
					ir[ID]=ir[IF];
				}
				if(raw_hazard==1)
				{
					ir[ID].opcode=NOP;
					raw_hazard_propagate=1;
					sp_registers[A][EXE]=UNDEFINED;
					sp_registers[B][EXE]=UNDEFINED;
					sp_registers[IMM][EXE]=UNDEFINED;
					sp_registers[NPC][EXE]=UNDEFINED;
				}

			}	
		

			if(is_memory(ir[IF].opcode))
			{
				if(ir[IF].opcode==LW)
				{	
					if(ir[IF].src1==ir[EXE].dest)
					{
						raw_hazard=1;
						stalls++;
					}
	
					if(ir[IF].src1==ir[MEM].dest)
					{
						raw_hazard=1;
						stalls++;
					}

					if (raw_hazard==0)
					{

						sp_registers[A][EXE]=get_gp_register(ir[IF].src1);
						sp_registers[B][EXE]=UNDEFINED;
						if(/*(0x8000 & ir[IF].immediate)==0x0000*/ ir[IF].immediate >= 0)
						{
							sp_registers[IMM][EXE]=0x00000000 | ir[IF].immediate;
						}
						if (/*(0x8000 & ir[IF].immediate) == 0x8000*/ ir[IF].immediate < 0)
						{
							sp_registers[IMM][EXE]=0xFFFF0000 | ir[IF].immediate;
						}
						sp_registers[NPC][EXE]=sp_registers[NPC][ID];
						ir[ID]=ir[IF];
					}
					if(raw_hazard==1)
					{
						raw_hazard_propagate=1;
						sp_registers[A][EXE]=UNDEFINED;
						sp_registers[B][EXE]=UNDEFINED;
						sp_registers[NPC][EXE]=UNDEFINED;
						sp_registers[IMM][EXE]=UNDEFINED;
						ir[ID].opcode = NOP;
					}
				}
			
				if (ir[IF].opcode==SW)
				{
					if(ir[IF].src1==ir[EXE].dest || ir[IF].src2==ir[EXE].dest)
					{

						raw_hazard=1;
						stalls++;
					}

					if(ir[IF].src1==ir[MEM].dest || ir[IF].src2==ir[MEM].dest)
					{
						raw_hazard=1;
						stalls++;
					}
					if(raw_hazard==0)
					{
						sp_registers[A][EXE]= get_gp_register(ir[IF].src1);
						sp_registers[B][EXE]= get_gp_register(ir[IF].src2);
						if(/*(0x8000 & ir[IF].immediate)==0x0000*/ ir[IF].immediate >= 0)
						{
							sp_registers[IMM][EXE]=0x00000000 | ir[IF].immediate;
						}
						if (/*(0x8000 & ir[IF].immediate) == 0x8000*/ ir[IF].immediate < 0)
						{
							sp_registers[IMM][EXE]=0xFFFF0000| ir[IF].immediate;
						}
						sp_registers[NPC][EXE]=sp_registers[NPC][ID];
						ir[ID]=ir[IF];
					}
					if(raw_hazard==1)
					{
						raw_hazard_propagate=1;
						ir[ID].opcode = NOP;
						sp_registers[A][EXE]=UNDEFINED;
						sp_registers[B][EXE]=UNDEFINED;
						sp_registers[NPC][EXE]=UNDEFINED;
						sp_registers[IMM][EXE]=UNDEFINED;
					}

				}

			}

			if(is_branch(ir[IF].opcode))
			{	
				control_hazard=1;
//				cout << " In Branch code ID stage uses IF.opcode " << endl;
				if(ir[IF].src1==ir[EXE].dest)
				{
					raw_hazard=1;
					stalls++;
				}

				if(ir[IF].src1==ir[MEM].dest)
				{
					raw_hazard=1;
					stalls++;
				}

				if(raw_hazard==0)
				{

					sp_registers[A][EXE]=get_gp_register(ir[IF].src1);
					sp_registers[B][EXE]=UNDEFINED;
					if (/*(0x8000 & ir[IF].immediate)==0x0000*/ ir[IF].immediate >= 0)
					{
						sp_registers[IMM][EXE]=0x00000000 | ir[IF].immediate;
					}
					if (/*(0x8000 & ir[IF].immediate) == 0x8000*/ ir[IF].immediate < 0)
					{
//						cout << " before sign extension ir[IF].immediate 0x" << hex <<ir[IF].immediate <<endl;
						sp_registers[IMM][EXE]=0xFFFF0000 | ir[IF].immediate;

//						cout << " after sign extension [IMM][EXE] 0x" << hex <<sp_registers[IMM][EXE]<<endl;
					}
					sp_registers[NPC][EXE]=sp_registers[NPC][ID];
					ir[ID]=ir[IF];
				}
				if(raw_hazard==1)
				{
					raw_hazard_propagate=1;
					ir[ID].opcode=NOP;
					sp_registers[A][EXE]=UNDEFINED;
					sp_registers[B][EXE]=UNDEFINED;
					sp_registers[NPC][EXE]=UNDEFINED;
					sp_registers[IMM][EXE]=UNDEFINED;
				}
			}

			if(ir[IF].opcode==EOP)
			{
				ir[ID]=ir[IF];
				//sp_registers[IR][IF]=UNDEFINED;
				sp_registers[A][EXE] = UNDEFINED;
				sp_registers[B][EXE] = UNDEFINED;
				sp_registers[IMM][EXE] = UNDEFINED;
			}	

			if(ir[IF].opcode==NOP && control_hazard_propagate==1)
			{
				ir[ID]=ir[IF];
				control_hazard_propagate_2=1;
				control_hazard_propagate=0;
				sp_registers[A][EXE]=UNDEFINED;
				sp_registers[B][EXE]=UNDEFINED;
				sp_registers[IMM][EXE]=UNDEFINED;
				sp_registers[NPC][EXE]=UNDEFINED;
			}

	
		}
//		cout <<" Instruction at the end of ID stage opcode=> "<<ir[ID].opcode<< " A: "<< dec << sp_registers[A][EXE] << " B: " << dec << sp_registers[B][EXE] << " Imm: 0x" << hex  <<sp_registers[IMM][EXE] << " ir[ID].src1= " << ir[ID].src1 << " ir[ID].src2= " << ir[ID].src2 << " ir[ID].dest= " << ir[ID].dest << " ir[EXE].dest= "<< ir[EXE].dest << " ir[MEM].dest = " << ir[MEM].dest <<endl;
	
		

			// <suggestion: use the helper functions "is_branch", "is_int_r", ..., above to improve code readability/compactness>

		/*=================================================   IF stage  ================================================  */



		if(structural_mem_hazard==0)
		{
			if(raw_hazard==0) //data hazard check.Bit gets set in the ID stage when data hazards are detected. If no data hazard only then fetch next instruction
			{
				if (control_hazard==0)
				{

					if (sp_registers[COND][WB]==0)
					{
					
//						cout << " Branch Taken code end of IF " << " sp_res condition " << sp_registers[COND][WB] <<endl;
//						cout << "sp_register[ALU_OUTPUT][WB] "<< sp_registers[ALU_OUTPUT][WB] << endl;
				
						sp_registers[PC][IF]=sp_registers[ALU_OUTPUT][WB];
						ir[IF]= instr_memory[(sp_registers[PC][IF] - instr_base_address)/4];
						sp_registers[PC][IF]=sp_registers[PC][IF]+4;	
						sp_registers[NPC][ID]=sp_registers[PC][IF];
					}	

					else
					{	
//						cout << " Branch not taken code end of IF " << endl;
					
						ir[IF]=instr_memory[(sp_registers[PC][IF] - instr_base_address)/4];
						if(ir[IF].opcode!=EOP)
						{
							sp_registers[PC][IF]=sp_registers[PC][IF]+4;
					//		sp_registers[ALU_OUTPUT][WB]=sp_registers[PC][IF];	
							sp_registers[NPC][ID]=sp_registers[PC][IF];
						}
						if(ir[IF].opcode==EOP)
						{
							sp_registers[NPC][ID]=sp_registers[PC][IF];
							sp_registers[NPC][EXE]=sp_registers[PC][IF];
						}
					}
				
				}
	
				if (control_hazard==1)
				{
					ir[IF].opcode=NOP;
					ir[IF].src1=UNDEFINED;
					ir[IF].src2=UNDEFINED;
					ir[IF].dest=UNDEFINED;
					ir[IF].immediate=UNDEFINED;
		
					sp_registers[NPC][ID]=UNDEFINED;
					
					control_hazard_propagate=1;
					stalls++;
					cstalls++;
			//		cout << " Control Hazard =1 code " <<endl;
//				cout << " IF stage control hazard check " << endl;
//			       cout << " Stalls due to control Hazard " << dec << cstalls << endl;	
				}	
			}
			if(raw_hazard==1)
			{
				ir[IF]=ir[IF];
				//raw_hazard_propagate=1;
			}
		}

		if(structural_mem_hazard==1)
		{
			mem_hazard_pipe_freeze=1;
			if(raw_hazard==1)
			{
				ir[IF]=ir[IF];
//				cout << " end of IF stage where struct and raw hazard is detected" << endl;
			}
			if(raw_hazard==0)
			{
				pc_temp = sp_registers[PC][IF]+4;
				//structural_mem_hazard_propagate=1;*/
				ir[IF]=instr_memory[(sp_registers[PC][IF] - instr_base_address)/4];
				if(ir[IF].opcode!=EOP)
				{
				sp_registers[NPC][ID]=sp_registers[PC][IF]+4;
				}
				if(latency_tracker==data_memory_latency && ir[IF].opcode!=EOP)
				{
					sp_registers[PC][IF]=sp_registers[PC][IF]+4;
				//	sp_registers[NPC][ID]=sp_registers[PC][IF]+4;
				}
			}
		}
	
	

//	cout << " Instruction at the end of IF stage opcode=> "<<ir[IF].opcode << " ir[IF].src1= " <<ir[IF].src1 << " ir[IF].src2= "<<ir[IF].src2 << " ir[IF].imm= 0x" <<ir[IF].immediate <<" ir[IF].dest= "<<ir[IF].dest<<endl;
	
//	cout << " Raw Hazard = " << raw_hazard;
//	cout << " structural mem hazard = " << structural_mem_hazard;
//	cout << " control hazard = " << control_hazard <<endl <<endl;

	// <hint: when accessing the instruction memory, you will need to scale PC to an integer index as in the following pseudocode:>
			// ir[IF/ID] = instr_memory[(PC-instr_base_address)>>2] 

                /* =============== */
                /* END STAGES      */
                /* =============== */

		/* Other bookkeeping code */
                /* ====================== */

	//	cout << " Stalls count =  "<< dec << stalls << endl;
	//	cout << " Cycle Count = " << dec<< clock_cycles<<endl <<endl;	
		clock_cycles++; // increase clock cycles count
	}
}
