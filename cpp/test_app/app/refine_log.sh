#!/bin/sh

PRODUCERS=`ls producer-*.log 2> /dev/null | sed 's/producer-//; s/\.log$//'`
CONSUMERS=`ls consumer-*.log 2> /dev/null | sed 's/consumer-//; s/\.log$//'`
DATE=`date`
LOGSFOLDER=${DATE//" "/"_"}".logs"
NDNDRC="~/.ndnx/ndndrc"

mkdir -p $LOGSFOLDER

function catAndGrep {
cat $1 | grep $2 > $3
}

function refineConsumer {
CFOLDER=$LOGSFOLDER/consumer/$1
CLOG=consumer-$1.log
echo "* refining logs for consumer "$1" ["$CFOLDER"] ..."

mkdir -p $CFOLDER

catAndGrep $CLOG "vconsumer-pipeliner" $CFOLDER/pipeliner.log
echo "  pipeliner logs ready ["$CFOLDER/pipeliner.log"]"

catAndGrep $CLOG "cchannel-iqueue" $CFOLDER/iqueue.log
echo "  interest queue logs ready ["$CFOLDER/iqueue.log"]"

catAndGrep $CLOG "vconsumer-buffer" $CFOLDER/buffer.log
echo "  buffer logs ready ["$CFOLDER/buffer.log"]"

catAndGrep $CLOG "\[vconsumer\]" $CFOLDER/vconsumer.log
echo "  consumer logs ready ["$CFOLDER/vconsumer.log"]"

catAndGrep $CLOG "\[vconsumer-buffer-pqueue\]" $CFOLDER/pqueue.log
echo "  playout queue logs ready ["$CFOLDER/pqueue.log"]"

catAndGrep $CLOG "\[STAT.\]\[playout\]" $CFOLDER/playout.stat.log
echo "  playout statistics ready ["$CFOLDER/playout.stat.log"]"

catAndGrep $CLOG "\[STAT.\]" $CFOLDER/all.stat.log
echo "  all statistics ready ["$CFOLDER/all.stat.log"]"
}

function refineProducer {
PFOLDER=$LOGSFOLDER/producer/$1
PLOG=producer-$1.log

echo "* refining logs for producer "$1" ["$PFOLDER"]..."

mkdir -p $PFOLDER

catAndGrep $PLOG "\[vsender\]" $PFOLDER/vsender.log
echo "  producer logs ready ["$PFOLDER/vsender.log"]"

catAndGrep $PLOG "\[STAT.\]\[vsender\]" $PFOLDER/vsender.stat.log
echo "  producer statistics ready ["$PFOLDER/vsender.stat.log"]"
}

echo "copying logs into ["$LOGSFOLDER/raw"]"
mkdir -p $LOGSFOLDER/raw
cp *.log $LOGSFOLDER/raw

for consumer in $CONSUMERS
do
refineConsumer $consumer
done

for producer in $PRODUCERS
do
refineProducer $producer
done

echo "preparing logs archive..."

NDNDLOG="/tmp/ndnd.log"

if [ -e $NDNDRC ]; then
    LOGENTRY=`cat $NDNDRC | grep "NDND_LOG="`
    NDNDLOG=${LOGENTRY#NDND_LOG=}
fi

if [ -e $NDNDLOG ]; then
SIZE=`du -h $NDNDLOG`
read -p "copy ndnd log? ($SIZE) (y/n)[n] " -n 1
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
echo "copying ndnd log. this may take a while..."
cp -v /tmp/ndnd.log $LOGSFOLDER/raw
fi
fi

echo "compressing logs..."
tar -czf $LOGSFOLDER.tar.gz $LOGSFOLDER

echo "logs are stored in "$LOGSFOLDER.tar.gz