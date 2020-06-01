/************************************************************************************
 * 
 * Sporks, the learning, scriptable Discord bot!
 *
 * Copyright 2019 Craig Edwards <support@sporks.gg>
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

#include <sporks/modules.h>
#include <sporks/regex.h>
#include <string>
#include <cstdint>
#include <fstream>
#include <streambuf>
#include <sporks/stringops.h>
#include <sporks/statusfield.h>
#include "trivia.h"
#include "numstrs.h"
#include "webrequest.h"

/**
 * Module class for trivia system
 */

class TriviaModule : public Module
{
	PCRE* notvowel;
	PCRE* number_tidy_dollars;
	PCRE* number_tidy_nodollars;
	PCRE* number_tidy_positive;
	PCRE* number_tidy_negative;
	std::map<int64_t, state_t*> states;

public:
	TriviaModule(Bot* instigator, ModuleLoader* ml) : Module(instigator, ml)
	{
		srand(time(NULL) * time(NULL));
		ml->Attach({ I_OnMessage }, this);
		notvowel = new PCRE("/[^aeiou_]/", true);
		number_tidy_dollars = new PCRE("^([\\d\\,]+)\\s+dollars$");
		number_tidy_nodollars = new PCRE("^([\\d\\,]+)\\s+(.+?)$");
		number_tidy_positive = new PCRE("^[\\d\\,]+$");
		number_tidy_negative = new PCRE("^\\-[\\d\\,]+$");
		set_io_context(bot->io);
		//t();
	}

	virtual ~TriviaModule()
	{
		delete notvowel;
		delete number_tidy_dollars;
		delete number_tidy_nodollars;
		delete number_tidy_positive;
		delete number_tidy_negative;
	}

	virtual std::string GetVersion()
	{
		/* NOTE: This version string below is modified by a pre-commit hook on the git repository */
		std::string version = "$ModVer 0$";
		return "1.0." + version.substr(8,version.length() - 9);
	}

	virtual std::string GetDescription()
	{
		return "Trivia System";
	}

	std::string dec_to_roman(unsigned int decimal)
	{
		std::vector<int> numbers =  { 1, 4 ,5, 9, 10, 40, 50, 90, 100, 400, 500, 900, 1000 };
		std::vector<std::string> romans = { "I", "IV", "V", "IX", "X", "XL", "L", "XC", "C", "CD", "D", "CM", "M" };
		std::string result;
		for (int x = 12; x >= 0; x--) {
			while (decimal >= numbers[x]) {
				decimal -= numbers[x];
				result.append(romans[x]);
			}
		}
		return std::string("Roman numerals: ") + result;
	}

	std::string tidy_num(std::string num)
	{
		std::vector<std::string> param;
		if (number_tidy_dollars->Match(num, param)) {
			num = "$" + ReplaceString(param[1], ",", "");
		}
		if (num.length() > 1 && num[0] == '$') {
			num = ReplaceString(num, ",", "");
		}
		if (number_tidy_nodollars->Match(num, param)) {
			std::string numbers = param[1];
			std::string suffix = param[2];
			numbers = ReplaceString(numbers, ",", "");
			num = numbers + " " + suffix;
		}
		if (number_tidy_positive->Match(num) || number_tidy_negative->Match(num)) {
			num = ReplaceString(num, ",", "");
		}
		return num;
	}

	std::string conv_num(std::string datain)
	{
		std::map<std::string, int> nn = {
			{ "one", 1 },
			{ "two", 2 },
			{ "three", 3 },
			{ "four", 4 },
			{ "five", 5 },
			{ "six", 6 },
			{ "seven", 7 },
			{ "eight", 8 },
			{ "nine", 9 },
			{ "ten", 10 },
			{ "eleven", 11 },
			{ "twelve", 12 },
			{ "thirteen", 13 },
			{ "fourteen", 14 },
			{ "forteen", 14 },
			{ "fifteen", 15 },
			{ "sixteen", 16 },
			{ "seventeen", 17 },
			{ "eighteen", 18 },
			{ "nineteen", 19 },
			{ "twenty", 20 },
			{ "thirty", 30 },
			{ "fourty", 40 },
			{ "forty", 40 },
			{ "fifty", 50 },
			{ "sixty", 60 },
			{ "seventy", 70 },
			{ "eighty", 80 },
			{ "ninety", 90 }
		};
		if (datain.empty()) {
			datain = "zero";
		}
		datain = ReplaceString(datain, "  ", " ");
		datain = ReplaceString(datain, "-", "");
		datain = ReplaceString(datain, " and ", " ");
		int last = 0;
		int initial = 0;
		std::string currency;
		std::vector<std::string> nums;
		std::stringstream str(datain);
		std::string v;
		while ((str >> v)) {
			nums.push_back(v);
		}
		for (auto x = nums.begin(); x != nums.end(); ++x) {
			if (nn.find(lowercase(*x)) == nn.end() && !PCRE("million", true).Match(*x) && !PCRE("thousand", true).Match(*x) && !PCRE("hundred", true).Match(*x) && !PCRE("dollars", true).Match(*x)) {
				return "0";
			}
		}
		for (auto next = nums.begin(); next != nums.end(); ++next) {
			std::string nextnum = lowercase(*next);
			auto ahead = next;
			ahead++;
			std::string lookahead = "";
			if (ahead != nums.end()) {
				lookahead = *ahead;
			}
			if (nn.find(nextnum) != nn.end()) {
				last = nn.find(nextnum)->second;
			}
			if (PCRE("dollars", true).Match(nextnum)) {
				currency = "$";
				last = 0;
			}
			if (!PCRE("hundred|thousand|million", true).Match(lookahead)) {
				initial += last;
				last = 0;
			} else {
				if (PCRE("hundred", true).Match(lookahead)) {
					initial += last * 100;
					last = 0;
				} else if (PCRE("thousand", true).Match(lookahead)) {
					initial += last * 1000;
					last = 0;
				} else if (PCRE("million", true).Match(lookahead)) {
					initial += last * 1000000;
					last = 0;
				}
			}
		}
		return currency + std::to_string(initial);
	}

	std::string scramble(std::string str)
	{
		int x = str.length();
		for(int y = x; y > 0; y--) 
		{ 
			int pos = rand()%x;
			char tmp = str[y-1];
			str[y-1] = str[pos];
			str[pos] = tmp;
		}
		return std::string("Scrambled answer: ") + str;
	}

	bool isVowel(char c) 
	{ 
		return (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'); 
	}

	std::string piglatinword(std::string s) 
	{ 
		int len = s.length(); 
		int index = -1; 
		for (int i = 0; i < len; i++) { 
			if (isVowel(s[i])) { 
				index = i; 
				break; 
			} 
		} 
		if (index == -1) 
			return s;
  
		return s.substr(index) + s.substr(0, index) + "ay"; 
	}

	std::string piglatin(std::string s) {
		std::stringstream str(s);
		std::string word;
		std::string ret;
		while ((str >> word)) {
			ret.append(piglatinword(word)).append(" ");
		}
		return std::string("Pig latin: ") + ret;
	}

	std::string letterlong(std::string text)
	{
		text = ReplaceString(text, " ", "");
		if (text.length()) {
			return fmt::format("{} letters long. Starts with '{}' and ends with '{}'.", text.length(), text[0], text[text.length() - 1]);
		} else {
			return "An empty answer";
		}
	}

	std::string vowelcount(std::string text)
	{
		text = ReplaceString(lowercase(text), " ", "");
		int v = 0;
		for (auto x = text.begin(); x != text.end(); ++x) {
			if (isVowel(*x)) {
				++v;
			}
		}
		return fmt::format("{} letters long and contains {} vowels.", text.length(), v);
	}

	std::string numbertoname(int64_t number)
	{
		if (numstrs.find(number) != numstrs.end()) {
			return numstrs.find(number)->second;
		}
		return std::to_string(number);
	}

	std::string GetNearestNumber(int64_t number)
	{
		for (numstrs_t::reverse_iterator x = numstrs.rbegin(); x != numstrs.rend(); ++x) {
			if (x->first <= number) {
				return x->second;
			}
		}
		return "0";
	}

	int64_t GetNearestNumberVal(int64_t number)
	{
		for (numstrs_t::reverse_iterator x = numstrs.rbegin(); x != numstrs.rend(); ++x) {
			if (x->first <= number) {
				return x->first;
			}
		}
		return 0;
	}

	int min3(int x, int y, int z) 
	{ 
		return std::min(std::min(x, y), z); 
	} 
  
	int levenstein(std::string str1, std::string str2) 
	{
		// Create a table to store results of subproblems
		str1 = uppercase(str1);
		str2 = uppercase(str2);
		int m = str1.length();
		int n = str2.length();
		int dp[m + 1][n + 1];

		// Fill d[][] in bottom up manner
		for (int i = 0; i <= m; i++) {
			for (int j = 0; j <= n; j++) {
				// If first string is empty, only option is to
				// insert all characters of second string
				if (i == 0)
					dp[i][j] = j; // Min. operations = j

				// If second string is empty, only option is to
				// remove all characters of second string
				else if (j == 0)
					dp[i][j] = i; // Min. operations = i

				// If last characters are same, ignore last char
				// and recur for remaining string
				else if (str1[i - 1] == str2[j - 1])
					dp[i][j] = dp[i - 1][j - 1];

				// If the last character is different, consider all
				// possibilities and find the minimum
				else
					dp[i][j] = 1 + min3(dp[i][j - 1], // Insert
						   dp[i - 1][j], // Remove
						   dp[i - 1][j - 1]); // Replace
			}
		}
		return dp[m][n]; 
	}

	bool is_number(const std::string &s)
	{
		return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
	}

	std::string MakeFirstHint(std::string s, bool indollars = false)
	{
		std::string Q;
		if (is_number(s)) {
			int64_t n = from_string<int64_t>(s, std::dec);
			while (GetNearestNumberVal(n) != 0 && n > 0) {
				Q.append(GetNearestNumber(n)).append(", plus ");
				n -= GetNearestNumberVal(n);
			}
			if (n > 0) {
				Q.append(numbertoname(n));
			}
			Q = Q.substr(0, Q.length() - 7);
		}
		if (Q.empty()) {
			return "The lowest non-negative number";
		}
		if (indollars) {
			return Q + ", in DOLLARS";
		} else {
			return Q;
		}
	}

	void t()
	{
		std::cout << "\n\nMakeFirstHint(12345): " << MakeFirstHint("12345") << "\n";
		std::cout << "MakeFirstHint(0): " << MakeFirstHint("0") << "\n";
		std::cout << "dec_to_roman(15): " << dec_to_roman(15) << "\n";
		std::cout << "conv_num('two thousand one hundred and fifty four'): " << conv_num("two thousand one hundred and fifty four") << "\n";
		std::cout << "conv_num('five'): " << conv_num("five") << "\n";
		std::cout << "conf_num('ten pin bowling'): " << conv_num("ten pin bowling") << "\n";
		std::cout << "conv_num('zero'): " << conv_num("zero") << "\n";
		std::cout << "scramble('abcdef'): " << scramble("abcdef") <<"\n";
		std::cout << "scramble('A'): " << scramble("A") << "\n";
       		std::cout << "piglatin('easy with the pig latin my friend'): " << piglatin("easy with the pig latin my friend") << "\n";
		std::cout << "conv_num('one million dollars'): " << conv_num("one million dollars") << "\n";
		std::cout << "tidy_num('$1,000,000'): " << tidy_num("$1,000,000") << "\n";
		std::cout << "tidy_num('1,000'): " << tidy_num("1,000") << "\n";
		std::cout << "tidy_num('1000'): " << tidy_num("1000") << "\n";
		std::cout << "tidy_num('asdfghjk'): " << tidy_num("asdfghjk") << "\n";
		std::cout << "tidy_num('abc def ghi'): " << tidy_num("abc def ghi") << "\n";
		std::cout << "tidy_num('1000 dollars') " << tidy_num("1000 dollars") << "\n";
		std::cout << "tidy_num('1,000 dollars') " << tidy_num("1,000 dollars") << "\n";
		std::cout << "tidy_num('1,000 armadillos') " << tidy_num("1,000 armadillos") << "\n";
		std::cout << "tidy_num('27 feet') " << tidy_num("27 feet") << "\n";
		std::cout << "tidy_num('twenty seven feet') " << tidy_num("twenty seven feet") << "\n";
		std::cout << "letterlong('a herd of gnus') " << letterlong("a herd of gnus") << "\n";
		std::cout << "vowelcount('a herd of gnus') " << vowelcount("a herd of gnus") << "\n";
		std::cout << "levenstein('a herd of cows','a herd of wocs') " << levenstein("a herd of cows","a herd of wocs") << "\n";
		std::cout << "levenstein('Cows','coWz')  " << levenstein("Cows","coWz") << "\n";
		exit(0);
	}

	virtual bool OnMessage(const modevent::message_create &message, const std::string& clean_message, bool mentioned, const std::vector<std::string> &stringmentions)
	{
		std::vector<std::string> param;
		std::string botusername = bot->user.username;
		aegis::gateway::objects::message msg = message.msg;
		const aegis::user& user = message.get_user();
		bool game_in_progress = false;

		const std::string prefix = "!";

		std::string trivia_message = clean_message;
		int x = from_string<int>(conv_num(clean_message), std::dec);
		if (x > 0) {
			trivia_message = conv_num(clean_message);
		}
		trivia_message = tidy_num(trivia_message);

		/* Retrieve current state for channel, if there is no state object, no game is in progress */
		int64_t channel_id = msg.get_channel_id().get();
		auto state_iter = states.find(channel_id);

		state_t* state = nullptr;
		if (state_iter != states.end()) {
			state = state_iter->second;
			game_in_progress = true;
		}

		if (game_in_progress) {
			if (state->gamestate == TRIV_ASK_QUESTION || state->gamestate == TRIV_FIRST_HINT || state->gamestate == TRIV_SECOND_HINT || state->gamestate == TRIV_TIME_UP) {
				
				if (state->round % 10 == 0) {
					/* Insane round */
				} else {
					/* Normal round */

					/* Answer on channel is an exact match for the current answer and/or it is numeric, OR, it's non-numeric and has a levenstein distance near enough to the current answer (account for misspellings) */
					if ((trivia_message.length() >= state->curr_answer.length() && lowercase(state->curr_answer) == lowercase(trivia_message)) || (!PCRE("^\\$(\\d+)$").Match(state->curr_answer) && !PCRE("^(\\d+)$").Match(state->curr_answer) && levenstein(trivia_message, state->curr_answer) < 2)) {
						/* Correct answer */
						state->round++;
						time_t time_to_answer = time(NULL) - state->asktime;

					}
				}

			}
		}

		if (lowercase(clean_message.substr(0, prefix.length())) == lowercase(prefix)) {
			/* Command */
			std::string command = clean_message.substr(prefix.length(), clean_message.length() - prefix.length());
			aegis::channel* c = bot->core.find_channel(msg.get_channel_id().get());
			if (c) {
				if (!bot->IsTestMode() || from_string<uint64_t>(Bot::GetConfig("test_server"), std::dec) == c->get_guild().get_id()) {


					std::stringstream tokens(command);

					std::string base_command;
					std::string subcommand;
				
					tokens >> base_command;

					if (lowercase(base_command) == "trivia") {
						tokens >> subcommand;
						subcommand = lowercase(subcommand);

						if (subcommand == "start") {

							int32_t questions;
							tokens >> questions;

							if (!game_in_progress) {
								if (questions < 5 || questions > 200) {
									c->create_message(fmt::format("**{}**, you can't create a normal trivia round of less than 5 or more than 200 questions!", user.get_username()));
									return false;
								}

								std::vector<std::string> sl = fetch_shuffle_list();
								if (sl.size() < 50) {
									aegis::channel* c = bot->core.find_channel(msg.get_channel_id().get());
									if (c)
										c->create_message(fmt::format("**{}**, something spoopy happened. Please try again in a couple of minutes!", user.get_username()));
									return false;
								} else  {

									state = new state_t();
									states[channel_id] = state;
									state->gamestate = TRIV_ASK_QUESTION;
									state->numquestions = questions + 1;
									state->round = 1;
									state->interval = 20;
									aegis::channel* c = bot->core.find_channel(msg.get_channel_id().get());
									if (c) {
										c->create_message(fmt::format("**{}** started a trivia round of **{}** questions!\n**First** question coming up!", user.get_username(), questions));
									}
	
									return false;
								}

							} else {
								c->create_message(fmt::format("Buhhh... a round is already running here, **{}**!", user.get_username()));
								return false;
							}
						} else if (subcommand == "stop") {
						}
					}

					/*c->create_message(fmt::format("received command='{}' test response='``0:{} 1:{} 2:{}``'", command, shuf[0], shuf[1], shuf[2]));
					bot->sent_messages++;*/
				}

			}

		}

		return true;
	}
};

ENTRYPOINT(TriviaModule);

