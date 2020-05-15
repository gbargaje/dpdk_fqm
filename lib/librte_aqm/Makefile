# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 NITK Surathkal

include $(RTE_SDK)/mk/rte.vars.mk

LIB = librte_aqm.a

CFLAGS += $(WERROR_FLAGS) -O3
LDLIBS += -lrte_eal -lrte_mbuf

EXPORT_MAP := rte_aqm_version.map

SRCS-$(CONFIG_RTE_LIBRTE_AQM) := circular_queue.c rte_aqm.c

SYMLINK-$(CONFIG_RTE_LIBRTE_AQM)-include := rte_aqm.h rte_aqm_algorithm.h

include $(RTE_SDK)/mk/rte.lib.mk
