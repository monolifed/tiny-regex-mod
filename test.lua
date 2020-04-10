local _sf = string.format
local commandfmt = [[./match '%s' '%s']]
local printfmt = [[%s : "%s", "%s", %s]]

local _x = function(s)
	return s:gsub('\\', '\\\\'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
end

local OK  = "OK"
local NOK = "NOK"
local exit_status = true

local test_tre = function(expected, pattern, sample)
	local result
	local pipe = io.popen(string.format(commandfmt, pattern, sample))
	pipe:read('*all')
    local _, exit, match = pipe:close()
	
	pattern, sample = _x(pattern), _x(sample)
	if exit == "exit" then
		if (match == 1 and expected == OK) or (match == 0 and expected == NOK) then 
			result = "PASS"
		else
			result = "FAIL"
			exit_status = false
		end
		print(_sf(printfmt, result, pattern, sample, expected))
	else
		exit_status = false
		print(_sf(printfmt, "ERROR", pattern, sample, expected))
		print(_sf("\t%s code:%s", exit, match))
	end
end


local test_vector =
{
	{ OK,  "\\d",                       "5"                },
	{ OK,  "\\w+",                      "hej"              },
	{ OK,  "\\s",                       "\t \n"            },
	{ NOK, "\\S",                       "\t \n"            },
	{ OK,  "[\\s]",                     "\t \n"            },
	{ NOK, "[\\S]",                     "\t \n"            },
	{ NOK, "\\D",                       "5"                },
	{ NOK, "\\W+",                      "hej"              },
	{ OK,  "[0-9]+",                    "12345"            },
	{ OK,  "\\D",                       "hej"              },
	{ NOK, "\\d",                       "hej"              },
	{ OK,  "[^\\w]",                    "\\"               },
	{ OK,  "[\\W]",                     "\\"               },
	{ NOK, "[\\w]",                     "\\"               },
	{ OK,  "[^\\d]",                    "d"                },
	{ NOK, "[\\d]",                     "d"                },
	{ NOK, "[^\\D]",                    "d"                },
	{ OK,  "[\\D]",                     "d"                },
	{ OK,  "^.*\\\\.*$",                "c:\\Tools"        },
	{ OK,  "^[\\+-]*[\\d]+$",           "+27"              },
	{ OK,  "[abc]",                     "1c2"              },
	{ NOK, "[abc]",                     "1C2"              },
	{ OK,  "[1-5]+",                    "0123456789"       },
	{ OK,  "[.2]",                      "1C2"              },
	{ OK,  "a*$",                       "Xaa"              },
	{ OK,  "a*$",                       "Xaa"              },
	{ OK,  "[a-h]+",                    "abcdefghxxx"      },
	{ NOK, "[a-h]+",                    "ABCDEFGH"         },
	{ OK,  "[A-H]+",                    "ABCDEFGH"         },
	{ NOK, "[A-H]+",                    "abcdefgh"         },
	{ OK,  "[^\\s]+",                   "abc def"          },
	{ OK,  "[^fc]+",                    "abc def"          },
	{ OK,  "[^d\\sf]+",                 "abc def"          },
	{ OK,  "\n",                        "abc\ndef"         },
	{ OK,  "b.\\s*\n",                  "aa\r\nbb\r\ncc\r\n\r\n" },
	{ OK,  ".*c",                       "abcabc"           },
	{ OK,  ".+c",                       "abcabc"           },
	{ OK,  "[b-z].*",                   "ab"               },
	{ OK,  "b[k-z]*",                   "ab"               },
	{ NOK, "[0-9]",                     "  - "             },
	{ OK,  "[^0-9]",                    "  - "             },
	{ OK,  "0|",                        "0|"               },
	{ NOK, "\\d\\d:\\d\\d:\\d\\d",      "0s:00:00"         },
	{ NOK, "\\d\\d:\\d\\d:\\d\\d",      "000:00"           },
	{ NOK, "\\d\\d:\\d\\d:\\d\\d",      "00:0000"          },
	{ NOK, "\\d\\d:\\d\\d:\\d\\d",      "100:0:00"         },
	{ NOK, "\\d\\d:\\d\\d:\\d\\d",      "00:100:00"        },
	{ NOK, "\\d\\d:\\d\\d:\\d\\d",      "0:00:100"         },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "0:0:0"            },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "0:00:0"           },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "0:0:00"           },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "00:0:0"           },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "00:00:0"          },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "00:0:00"          },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "0:00:00"          },
	{ OK,  "\\d\\d?:\\d\\d?:\\d\\d?",   "00:00:00"         },
	{ OK,  "[Hh]ello [Ww]orld\\s*[!]?", "Hello world !"    },
	{ OK,  "[Hh]ello [Ww]orld\\s*[!]?", "hello world !"    },
	{ OK,  "[Hh]ello [Ww]orld\\s*[!]?", "Hello World !"    },
	{ OK,  "[Hh]ello [Ww]orld\\s*[!]?", "Hello world!   "  },
	{ OK,  "[Hh]ello [Ww]orld\\s*[!]?", "Hello world  !"   },
	{ OK,  "[Hh]ello [Ww]orld\\s*[!]?", "hello World    !" },
	{ NOK, "\\d\\d?:\\d\\d?:\\d\\d?",   "a:0"              },
--[[]]
	{ OK,  "[^\\w][^-1-4]",     ")T"          },
	{ OK,  "[^\\w][^-1-4]",     ")^"          },
	{ OK,  "[^\\w][^-1-4]",     "*)"          },
	{ OK,  "[^\\w][^-1-4]",     "!."          },
	{ OK,  "[^\\w][^-1-4]",     " x"          },
	{ OK,  "[^\\w][^-1-4]",     "$b"          },
--[[]]
	{ OK,  ".?bar",                      "real_bar"        },
	{ NOK, ".?bar",                      "real_foo"        },
	{ NOK, "X?Y",                        "Z"               },
}


for i, v in pairs(test_vector) do test_tre(table.unpack(v)) end
os.exit(exit_status)