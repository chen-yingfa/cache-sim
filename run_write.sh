write_policies=(
    "back_alloc"
    "back_noAlloc"
    "through_alloc"
    "through_noAlloc"
)

for write_policy in ${write_policies[@]}
do
    write_policy=$write_policy ./run.sh 
done