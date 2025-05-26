//////////////////////////////// DRRIP Replcement Policy ////////////////////////////////


/*
    The cache is partitioned into
        - Leader set: hardwired to use SRRIP or BRRIP
        - Follower sets: follow the currently best-performing policy

    DRRIP switches between SRRIP and BIP based on runtime behavior using
    set dueling and a policy selector counter (PSEL)
*/

#include "../inc/champsim_crc2.h"
#include <vector>
#include <map>
#include <random>
#include <set>
#include <cassert>
#include <iostream>
#include <algorithm>

#define NUM_CORE 1
#define LLC_SETS NUM_CORE*2048
#define LLC_WAYS 16

#define RRPV_MAX 3
#define NUM_POLICY 2
#define SDM_SIZE 32             // leader set count per policy per core
#define TOTAL_SDM_SET NUM_POLICY*SDM_SIZE
#define MAX_BIP 32
#define PSEL_WIDTH 10
#define PSEL_MAX 1023
#define PSEL_INIT (PSEL_MAX / 2)

uint32_t rrpv[LLC_SETS][LLC_WAYS];

int bip_counter = 0;                
int psel = PSEL_INIT;               // 0 is SRRIP, PSEL_MAX is BRRIP
vector<int> bip_leader_sets;
vector<int> srrip_leader_sets;

// try a more dynamic psel counter
uint64_t PHASE_LENGTH = 100000;
uint64_t global_access_counter = 0;
uint64_t next_phase_start = PHASE_LENGTH;

// helper functions to check if the cache set is either policy leader
bool is_leader(const vector<int>& leaders, int set){
    return std::find(leaders.begin(), leaders.end(), set) != leaders.end();
}
bool is_bip_leader(int set){return is_leader(bip_leader_sets, set);}
bool is_srrip_leader(int set){return is_leader(srrip_leader_sets, set);}
bool is_follower(int set){return !is_bip_leader(set) && !is_srrip_leader(set);}

/*
    Randomly select sampler sets
*/
void InitReplacementState()
{
    cout << "Initialize DRRIP replacement state" << endl;

    for(int i = 0; i < LLC_SETS; i++){
        for(int j = 0; j < LLC_WAYS; j++){
            rrpv[i][j] = RRPV_MAX;
        }
    }

    // set random seed
    mt19937 rng(42);
    uniform_int_distribution<int> dist(0, LLC_SETS - 1);
    set<int> used;

    // randomly select the BRRIP leader set
    while(bip_leader_sets.size() < SDM_SIZE){
        int val = dist(rng);
        if(used.insert(val).second) bip_leader_sets.push_back(val);
    }

    // select the SRRIP leader set indice
    while(srrip_leader_sets.size() < SDM_SIZE){
        int val = dist(rng);
        if(used.insert(val).second) srrip_leader_sets.push_back(val);
    }
}

// find replacement victim
// return value should be 0 ~ 15 or 16 (bypass)
uint32_t GetVictimInSet (uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type)
{
    while(1){
        for(int i = 0; i < LLC_WAYS; i++){
            if(rrpv[set][i] == RRPV_MAX) return i;
        }
        for(int i = 0; i < LLC_WAYS; i++){
            rrpv[set][i]++;
        }
    }

    cerr << "[ERROR] DRRIP victim selection failed — no block with MAX_RRPV" << endl;
    exit(1);
}

// called on every cache hit and cache fill
void UpdateReplacementState (uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    // I tried to use global counter to neutralize psel over some cycles
    global_access_counter++;
    if(global_access_counter >= next_phase_start){
        if(psel > PSEL_MAX/2) psel = min(PSEL_MAX/2 , psel-(psel >> 2));
        else psel = max(PSEL_MAX/2, psel+((PSEL_MAX-psel) >> 2));
        next_phase_start += PHASE_LENGTH;
    }

    if(type == WRITEBACK){
        rrpv[set][way] = RRPV_MAX - 1;
        return;
    }

    if(hit){
        rrpv[set][way] = 0;
        return;
    }

    // on cache miss
    else{
        // if it's the BRRIP leader set, penalty the psel to SRRIP
        if(is_bip_leader(set)){
            // cout << "entered BRRIP SDM set" << endl;
            psel = max(0, psel - 1);
            rrpv[set][way] = RRPV_MAX;
            if(++bip_counter == MAX_BIP){
                bip_counter = 0;
                rrpv[set][way] = RRPV_MAX - 1;
            }
        }
        // if it's the SRRIP leader set, increase psel
        else if(is_srrip_leader(set)){
            // cout << "entered SRRIP SDM set" << endl;
            psel = min(PSEL_MAX, psel + 10);
            rrpv[set][way] = RRPV_MAX - 1;
        }
        else{
            if(psel > (PSEL_MAX/2)){
                rrpv[set][way] = RRPV_MAX - 1;
            }
            else{
                rrpv[set][way] = RRPV_MAX;
                // occasionally set the rrpv value to (RRPV_MAX-1)
                if(++bip_counter == MAX_BIP){
                    bip_counter = 0;
                    rrpv[set][way] = RRPV_MAX - 1;
                }
            }
        }
    }
}

// use this function to print out your own stats on every heartbeat 
void PrintStats_Heartbeat()
{

}

// use this function to print out your own stats at the end of simulation
void PrintStats()
{

}