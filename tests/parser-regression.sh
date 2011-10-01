cd $(dirname $(readlink -f "$0" ))
#
#for inputfile in \
#../src/parser/samples/{mpeg2ffplay,xcl,ephy,p2k}.cake \
#../examples/*/*.cake \
#; do
#python ../src/parser/cakePyParser.py --rule=toplevel | \
#    ~/work/devel/antlr/parens-filter.sh || exit 1
#done

make -C ../src/parser test-samples
