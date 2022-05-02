current_path=$(cd `dirname $0`; pwd)
perf record -F99 -g $1
perf script | $current_path/../tools/FlameGraph/stackcollapse-perf.pl > out.perf-folded
$current_path/../tools/FlameGraph/flamegraph.pl out.perf-folded > out.svg

