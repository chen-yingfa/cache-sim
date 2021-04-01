replace_policy=(
    "binTree"
    "LRU"
    "PLRU"
)

for replace in ${replace_policy[@]}
do
    replace_policy=$replace ./run.sh
done