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

#ifndef	CONFIG_CONFIG_OBJECT_H
#define	CONFIG_CONFIG_OBJECT_H

#include <config/config_class.h>
#include <config/config_exporter.h>

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           config_object.h                                            //
// Description:    parser for configuration objects                           //
// Project:        WANProxy XTech                                             //
// Adapted by:     Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

class Config;

struct ConfigObject {
	Config *config_;
	std::string name_;
	const ConfigClass *class_;
	ConfigClassInstance *instance_;

	ConfigObject(Config *config, const std::string& name, const ConfigClass *cc, ConfigClassInstance *inst)
	: config_(config),
	  name_(name),
	  class_(cc),
	  instance_(inst)
	{ }

	virtual ~ConfigObject()
	{
		delete instance_;
	}

	bool activate(void) const;
	void marshall(ConfigExporter *) const;
	bool set(const std::string&, const std::string&);
};

template<typename Tc, typename Tf, typename Ti>
void ConfigClass::add_member(const std::string& mname, Tc type, Tf Ti::*fieldp)
{
	struct TypedConfigClassMember : public ConfigClassMember {
		Tc config_type_;
		Tf Ti::*config_field_;

		TypedConfigClassMember(Tc config_type, Tf Ti::*config_field)
		: config_type_(config_type),
		  config_field_(config_field)
		{ }
		virtual ~TypedConfigClassMember()
		{ }

		void marshall(ConfigExporter *exp, const ConfigClassInstance *instance) const
		{
			const Ti *inst = dynamic_cast<const Ti *>(instance);
			ASSERT("/config/class/field", inst != NULL);
			config_type_->marshall(exp, &(inst->*config_field_));
		}

		bool set(ConfigObject *co, const std::string& vstr) const
		{
			Ti *inst = dynamic_cast<Ti *>(co->instance_);
			ASSERT("/config/class/field", inst != NULL);
			return (config_type_->set(co, vstr, &(inst->*config_field_)));
		}

		ConfigType *type(void) const
		{
			return (config_type_);
		}
	};

	ASSERT("/config/class/" + name_, members_.find(mname) == members_.end());
	members_[mname] = new TypedConfigClassMember(type, fieldp);
}

#endif /* !CONFIG_CONFIG_OBJECT_H */
