/*
 * Copyright (c) 2009-2013 Juli Mallett. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	CONFIG_CONFIG_CLASS_H
#define	CONFIG_CONFIG_CLASS_H

#include <map>

#include <common/factory.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           config_class.h                                             //
// Description:    parser for configuration classes                           //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class Config;
class ConfigExporter;
class ConfigType;
class ConfigObject;

/*
 * A base type for class instances.
 */
class ConfigClassInstance {
protected:
	ConfigClassInstance(void)
	{ }
public:
	virtual ~ConfigClassInstance()
	{ }

	virtual bool activate(const ConfigObject *) = 0;
};

class ConfigClassMember {
protected:
	ConfigClassMember(void)
	{ }
public:
	virtual ~ConfigClassMember()
	{ }

	virtual void marshall(ConfigExporter *, const ConfigClassInstance *) const = 0;
	virtual bool set(ConfigObject *, const std::string&) const = 0;
	virtual ConfigType *type(void) const = 0;
};

class ConfigClass {
	friend class Config;
	friend struct ConfigObject;

	std::string name_;
	Factory<ConfigClassInstance> *factory_;
	std::map<std::string, const ConfigClassMember *> members_;
protected:
	ConfigClass(const std::string& xname, Factory<ConfigClassInstance> *factory)
	: name_(xname),
	  factory_(factory),
	  members_()
	{ }

	virtual ~ConfigClass();

	template<typename Tc, typename Tf, typename Ti>
	void add_member(const std::string& mname, Tc type, Tf Ti::*fieldp);

private:
	ConfigClassInstance *allocate(void) const
	{
		return (factory_->create());
	}

	bool set(ConfigObject *, const std::string&, const std::string&) const;

public:
	void marshall(ConfigExporter *, const ConfigClassInstance *) const;

	std::string name(void) const
	{
		return (name_);
	}
};

#endif /* !CONFIG_CONFIG_CLASS_H */
