# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 NITK Surathkal

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_aqm.a

CFLAGS += $(WERROR_FLAGS) -O3 -Wno-unused-parameter
LDLIBS += -lrte_eal -lrte_mbuf
LDLIBS += -lm

EXPORT_MAP := rte_aqm_version.map

SRCS-$(CONFIG_RTE_LIBRTE_AQM) := circular_queue.c rte_aqm.c
SRCS-$(CONFIG_RTE_LIBRTE_AQM) += aqm_red.c aqm_wred.c
SRCS-$(CONFIG_RTE_LIBRTE_AQM) += aqm_pie.c aqm_codel.c
SRCS-$(CONFIG_RTE_LIBRTE_AQM) += rte_red.c rte_pie.c

SYMLINK-$(CONFIG_RTE_LIBRTE_AQM)-include := rte_aqm.h rte_aqm_algorithm.h
SYMLINK-$(CONFIG_RTE_LIBRTE_AQM)-include += rte_red.h rte_pie.h

include $(RTE_SDK)/mk/rte.lib.mk
