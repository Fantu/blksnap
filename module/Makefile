# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Veeam Software Group GmbH

# ccflags-y += -D DEBUG
# ccflags-y += -D CONFIG_BLKSNAP_CHUNK_DBG
# ccflags-y += -D CONFIG_BLKSNAP_COW_SCHEDULE
ccflags-y += -D BLKSNAP_STANDALONE
ccflags-y += -D BLKSNAP_MODIFICATION
ccflags-y += -D BLKSNAP_MEMSTAT
ccflags-y += -D BLKSNAP_FILELOG
ccflags-y += -D BLKSNAP_HISTOGRAM

include ${M}/Makefile-config
include ${M}/Makefile-bdevfilter

veeamblksnap-y :=	\
	cbt_map.o	\
	chunk.o		\
	diff_area.o	\
	diff_buffer.o	\
	diff_storage.o	\
	event_queue.o	\
	main.o		\
	snapimage.o	\
	snapshot.o	\
	tracker.o

veeamblksnap-y += log.o
veeamblksnap-y += log_histogram.o
veeamblksnap-y += memstat.o

obj-m	 += veeamblksnap.o
