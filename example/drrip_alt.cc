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

#define RRPV_MAX 7
#define NUM_POLICY 2
#define SDM_SIZE 32 // leader set count per policy per core
#define TOTAL_SDM_SET NUM_POLICY*SDM_SIZE
// #define MAX_BIP 32
// #define PSEL_WIDTH 10
#define PSEL_MAX 127
#define PSEL_INIT (PSEL_MAX / 2) // start in srrip

uint32_t rrpv[LLC_SETS][LLC_WAYS];

// int bip_counter = 0;
double brrip_eps = 0.1;

int psel = PSEL_INIT;               // 0 is SRRIP, PSEL_MAX is BRRIP
vector<uint32_t> brrip_leader_sets;
vector<uint32_t> srrip_leader_sets;

// try a more dynamic psel counter
uint64_t PHASE_LENGTH = 32;
uint64_t global_access_counter = 0;
uint64_t next_phase_start = PHASE_LENGTH;

// helper functions to check if the cache set is either policy leader
bool is_leader(const vector<uint32_t>& leaders, uint32_t set){
    return std::find(leaders.begin(), leaders.end(), set) != leaders.end();
}
bool is_bip_leader(uint32_t set){return is_leader(brrip_leader_sets, set);}
bool is_srrip_leader(uint32_t set){return is_leader(srrip_leader_sets, set);}
bool is_follower(uint32_t set){return !is_bip_leader(set) && !is_srrip_leader(set);}

/*
    Randomly select sampler sets
*/
void InitReplacementState()
{
    cout << "Initialize DRRIP replacement state" << endl;
    // Set all to max rrpv i.e. all can be replaced
    for(int i = 0; i < LLC_SETS; i++){
        for(int j = 0; j < LLC_WAYS; j++){
            rrpv[i][j] = RRPV_MAX;
        }
    }

    // set random seed
    mt19937 rng(5);
    uniform_int_distribution<int> dist(0, LLC_SETS - 1);
    set<int> used;
    srand(6); // for brrip

    // randomly select the BRRIP leader set
    while(brrip_leader_sets.size() < SDM_SIZE){
        int val = dist(rng);
        if(used.insert(val).second) brrip_leader_sets.push_back(val);
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
    // cout << "cpu " << cpu << " set " << set << " way " << way << " paddr " << paddr << " PC " << PC << " victim addr " << victim_addr << " type " << type << " hit " << (int) hit << endl;
    // I tried to use global counter to neutralize psel over some cycles
    // global_access_counter++;
    // if(global_access_counter >= next_phase_start){
    //     // penalize psel towards center but don't switch mode
    //     if(psel > PSEL_MAX/2) psel = PSEL_INIT+1;
    //     else psel = PSEL_INIT;
    //     next_phase_start += PHASE_LENGTH;
    // }

    if(type == WRITEBACK){
        rrpv[set][way] = RRPV_MAX - 1;
        return;
    }
    cout << "psel = " << psel << endl;
    if(hit){
        rrpv[set][way] = 0;
        // reward psel if is a leader
        if (is_bip_leader(set))
            psel = min(PSEL_MAX,psel+1);
        else if (is_srrip_leader(set))
            psel = max(0,psel-1);
        return;
    }

    // on cache miss
    else{
        // if it's the BRRIP leader set, penalize the psel towards SRRIP
        if(is_bip_leader(set)){
            // cout << "entered BRRIP SDM set" << endl;
            psel = max(0, psel - 1);
            rrpv[set][way] = RRPV_MAX-1;
            // randomly 
            double r = (double) rand() / RAND_MAX;
            // cout << "r = " << r << endl;
            if(r < brrip_eps){
                // bip_counter = 0;
                rrpv[set][way] = RRPV_MAX-2;
            }
        }
        // if it's the SRRIP leader set, update srrip subset, increase psel to BRRIP
        else if(is_srrip_leader(set)){
            // cout << "entered SRRIP SDM set" << endl;
            psel = min(PSEL_MAX, psel + 1);
            rrpv[set][way] = RRPV_MAX-2;
        }
        // Update follower buffers according to current policy
        else
        {
            // BRRIP
            if(psel > (PSEL_MAX/2)){
                rrpv[set][way] = RRPV_MAX-1;
                double r = (double) rand() / RAND_MAX;
                if(r < brrip_eps){
                    // bip_counter = 0;
                    rrpv[set][way] = RRPV_MAX-2;
                }
            }
            // SRRIP
            else{
                rrpv[set][way] = RRPV_MAX-2;
            }
        }
        return;
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