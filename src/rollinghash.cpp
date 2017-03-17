#include "rollinghash.h"
#include "builtinmutation.h"
#include "util.h"

RollingHash::RollingHash(int window, bool allowTwoSub) {
    mWindow = min(50, window);
    mAllowTwoSub = allowTwoSub;
}

bool RollingHash::add(string s, void* target) {
    if(s.length() < mWindow + 2)
        return false;

    int center = s.length() / 2;
    int start = center - mWindow / 2;
    
    // mutations cannot happen in skipStart to skipEnd
    int skipStart = center - 1;
    int skipEnd = center + 1;

    const char* data = s.c_str();

    long* hashes = new long[mWindow];
    memset(hashes, 0, sizeof(long)*mWindow);

    long* accum = new long[mWindow];
    memset(accum, 0, sizeof(long)*mWindow);

    // initialize
    long origin = 0;
    for(int i=0; i<mWindow; i++) {
        hashes[i] = hash(data[start + i], i);
        origin += hashes[i];
        accum[i] = origin;
    }
    addHash(origin, target);

    const char bases[4] = {'A', 'T', 'C', 'G'};

    // make subsitution hashes, we allow up to 2 sub mutations
    for(int i=0; i<mWindow; i++) {
        if(i+start >= skipStart && i+start <= skipEnd )
            continue;
        for(int b1=0; b1<4; b1++){
            char base1 = bases[b1];
            if(base1 == data[start + i])
                continue;

            long mut1 = origin - hash(data[start + i], i) + hash(base1, i);
            addHash(mut1, target);
            /*cout<<mut1<<":"<<i<<base1<<":";
            for(int p=0; p<mWindow; p++) {
                if(p==i)
                    cout<<base1;
                else
                    cout<<data[start + p];
            }
            cout<<endl;*/

            if(mAllowTwoSub) {
                for(int j=i+1; j<mWindow; j++) {
                    if(j+start >= skipStart && j+start <= skipEnd )
                        continue;
                    for(int b2=0; b2<4; b2++){
                        char base2 = bases[b2];
                        if(base2 == data[start + j])
                            continue;
                        long mut2 = mut1 - hash(data[start + j], j) + hash(base2, j);
                        addHash(mut2, target);
                        /*cout<<mut2<<":"<<i<<base1<<j<<base2<<":";
                        for(int p=0; p<mWindow; p++) {
                            if(p==i)
                                cout<<base1;
                            else if(p==j)
                                cout<<base2;
                            else
                                cout<<data[start + p];
                        }
                        cout<<endl;*/
                    }
                }
            }
        }
    }

    int altForDel = start - 1;
    long altVal = hash(data[altForDel], 0);

    // make indel hashes, we only allow 1 indel
    for(int i=0; i<mWindow; i++) {
        if(i+start >= skipStart && i+start <= skipEnd )
            continue;
        // make del of i first
        long mutOfDel;
        if (i==0)
            mutOfDel = origin - accum[i] + altVal;
        else
            mutOfDel = origin - accum[i] + (accum[i-1]<<1) + altVal;
        if(mutOfDel != origin)
            addHash(mutOfDel, target);

        // make insertion
        for(int b=0; b<4; b++){
            char base = bases[b];
            // shift the first base
            long mutOfIns = origin - accum[i] + hash(base, i) + ((accum[i] - hashes[0]) >> 1);
            if(mutOfIns != origin && mutOfIns != mutOfDel){
                addHash(mutOfIns, target);
                /*cout << mutOfIns<<", insert at " << i << " with " << base << ": ";
                for(int p=1;p<=i;p++)
                    cout << data[start + p];
                cout << base;
                for(int p=i+1;p<mWindow;p++)
                    cout << data[start + p];
                cout << endl;*/
            }
        }
    }

    delete hashes;
    delete accum;
}

void RollingHash::addHash(long hash, void* target) {
    if(mKeyTargets.count(hash) == 0)
        mKeyTargets[hash] = vector<void*>();
    else {
        for(int i=0; i<mKeyTargets[hash].size(); i++) {
            if(mKeyTargets[hash][i] == target)
                return ;
        }
    }

    mKeyTargets[hash].push_back(target);
}

inline long RollingHash::char2val(char c) {
    switch (c) {
        case 'A':
            return 517;
        case 'T':
            return 433;
        case 'C':
            return 1123;
        case 'G':
            return 127;
        case 'N':
            return 1;
        default:
            return 0;
    }
}

inline long RollingHash::hash(char c, int pos) {
    long val = char2val(c);
    const long num = 2;
    return val * (num << pos );
}

void RollingHash::dump() {
    map<long, vector<void*> >::iterator iter;
    for(iter= mKeyTargets.begin(); iter!=mKeyTargets.end(); iter++) {
        if(iter->second.size() < 2)
            continue;
        cout << iter->first << endl;
        for(int i=0; i<iter->second.size(); i++)
            cout << (long)iter->second[i] << "\t";
        cout << endl;
    }
}
bool RollingHash::test(){
    RollingHash hasher(50);
    vector<string> lines;
    split(BUILT_IN_MUTATIONS, lines, "\n");
    for(int i=0;i<lines.size();i++){
        string linestr = lines[i];
        vector<string> splitted;
        split(linestr, splitted, ",");
        // a valid line need 4 columns: name, left, center, right
        if(splitted.size()<4)
            continue;
        // comment line
        if(starts_with(splitted[0], "#"))
            continue;
        string name = trim(splitted[0]);
        string left = trim(splitted[1]);
        string center = trim(splitted[2]);
        string right = trim(splitted[3]);
        cout << i << ":" << name << endl;
        hasher.add(left+center+right, (void*)i);
    }
    hasher.dump();
    return true;

}