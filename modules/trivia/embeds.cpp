/************************************************************************************
 * 
 * TriviaBot, The trivia bot for discord based on Fruitloopy Trivia for ChatSpike IRC
 *
 * Copyright 2004 Craig Edwards <support@brainbox.cc>
 *
 * Core based on Sporks, the Learning Discord Bot, Craig Edwards (c) 2019.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/

#include <string>
#include <cstdint>
#include <fstream>
#include <streambuf>
#include <sporks/modules.h>
#include <sporks/stringops.h>
#include <sporks/statusfield.h>
#include <sporks/database.h>
#include "trivia.h"

/* Make a string safe to send as a JSON literal */
std::string TriviaModule::escape_json(const std::string &s) {
	std::ostringstream o;
	for (auto c = s.cbegin(); c != s.cend(); c++) {
		switch (*c) {
		case '"': o << "\\\""; break;
		case '\\': o << "\\\\"; break;
		case '\b': o << "\\b"; break;
		case '\f': o << "\\f"; break;
		case '\n': o << "\\n"; break;
		case '\r': o << "\\r"; break;
		case '\t': o << "\\t"; break;
		default:
			if ('\x00' <= *c && *c <= '\x1f') {
				o << "\\u"
				  << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
			} else {
				o << *c;
			}
		}
	}
	return o.str();
}

/* Create an embed from a JSON string and send it to a channel */
void TriviaModule::ProcessEmbed(const guild_settings_t& settings, const std::string &embed_json, int64_t channelID)
{
	if (bot->core.find_channel(channelID)) {
		json embed;
		std::string cleaned_json = embed_json;
		/* Put unicode zero-width spaces in @everyone and @here */
		cleaned_json = ReplaceString(cleaned_json, "@everyone", "@‎everyone");
		cleaned_json = ReplaceString(cleaned_json, "@here", "@‎here");
		try {
			/* Tabs to spaces */
			cleaned_json = ReplaceString(cleaned_json, "\t", " ");
			embed = json::parse(cleaned_json);
		}
		catch (const std::exception &e) {
			if (!bot->IsTestMode() || from_string<uint64_t>(Bot::GetConfig("test_server"), std::dec) == channelID) {
					try {
					bot->core.create_message(channelID, fmt::format(_("EMBED_ERROR_1", settings), cleaned_json, e.what()));
				}
				catch (const std::exception &e) {
					bot->core.log->error("MALFORMED UNICODE: {}", e.what());
				}
				bot->sent_messages++;
			}
		}
		if (!bot->IsTestMode() || from_string<uint64_t>(Bot::GetConfig("test_server"), std::dec) == channelID) {
			bot->core.create_message_embed(channelID, "", embed);
			bot->sent_messages++;
		}
	}
}

void TriviaModule::SimpleEmbed(const guild_settings_t& settings, const std::string &emoji, const std::string &text, int64_t channelID, const std::string &title)
{
	uint32_t colour = settings.embedcolour;
	if (!title.empty()) {
		ProcessEmbed(settings, fmt::format("{{\"title\":\"{}\",\"color\":{},\"description\":\"{} {}\"}}", escape_json(title), colour, emoji, escape_json(text)), channelID);
	} else {
		ProcessEmbed(settings, fmt::format("{{\"color\":{},\"description\":\"{} {}\"}}", colour, emoji, escape_json(text)), channelID);
	}
}

/* Send an embed containing one or more fields */
void TriviaModule::EmbedWithFields(const guild_settings_t& settings, const std::string &title, std::vector<field_t> fields, int64_t channelID, const std::string &url)
{
		uint32_t colour = settings.embedcolour;
		std::string json = fmt::format("{{" + (!url.empty() ? "\"url\":\"" + escape_json(url) + "\"," : "") + "\"title\":\"{}\",\"color\":{},\"fields\":[", escape_json(title), colour);
		for (auto v = fields.begin(); v != fields.end(); ++v) {
			json += fmt::format("{{\"name\":\"{}\",\"value\":\"{}\",\"inline\":{}}}", escape_json(v->name), escape_json(v->value), v->_inline ? "true" : "false");
			auto n = v;
			if (++n != fields.end()) {
				json += ",";
			}
		}
		json += "],\"footer\":{\"link\":\"https://triviabot.co.uk/\",\"text\":\"" + _("POWERED_BY", settings) + "\",\"icon_url\":\"https:\\/\\/triviabot.co.uk\\/images\\/triviabot_tl_icon.png\"}}";
		ProcessEmbed(settings, json, channelID);
}

