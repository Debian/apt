/*
 * private-json-hooks.h - 2nd generation, JSON-RPC, hooks for APT
 *
 * Copyright (c) 2018 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <set>
#include <string>

#include <apt-private/private-cachefile.h>

bool RunJsonHook(std::string const &option, std::string const &method, const char **FileList, CacheFile &Cache, std::set<std::string> const &UnknownPackages = {});
