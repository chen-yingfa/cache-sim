blocksizes=(
    8 
    32 
    64
)
way_cnts=(
    1
    4
    8
    0 
)

make build

for blocksize in ${blocksizes[@]}
do
    for nways in ${way_cnts[@]}
    do
        blocksize=$blocksize \
        nways=$nways \
        ./run.sh
    done
done