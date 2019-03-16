/*
 *      Copyright (C) 2010-2019 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "DShowUtil.h"

#include <string>
#include <regex>
#include <algorithm>

static struct {LPCSTR name, iso6392, iso6391, iso6392_2; LCID lcid;} s_isolangs[] =	// TODO : fill LCID !!!
{
  {"Abkhazian", "abk", "ab"},
  {"Achinese", "ace", nullptr},
  {"Acoli", "ach", nullptr},
  {"Adangme", "ada", nullptr},
  {"Afar", "aar", "aa"},
  {"Afrihili", "afh", nullptr},
  {"Afrikaans", "afr", "af", nullptr, MAKELCID( MAKELANGID(LANG_AFRIKAANS, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Afro-Asiatic (Other)", "afa", nullptr},
  {"Akan", "aka", "ak"},
  {"Akkadian", "akk", nullptr},
  {"Albanian", "sqi", "sq", "alb", MAKELCID( MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Aleut", "ale", nullptr},
  {"Algonquian languages", "alg", nullptr},
  {"Altaic (Other)", "tut", nullptr},
  {"Amharic", "amh", "am"},
  {"Apache languages", "apa", nullptr},
  {"Arabic", "ara", "ar", nullptr, MAKELCID( MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Aragonese", "arg", "an"},
  {"Aramaic", "arc", nullptr},
  {"Arapaho", "arp", nullptr},
  {"Araucanian", "arn", nullptr},
  {"Arawak", "arw", nullptr},
  {"Armenian", "arm", "hy",	"hye", MAKELCID( MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Artificial (Other)", "art", nullptr},
  {"Assamese", "asm", "as", nullptr, MAKELCID( MAKELANGID(LANG_ASSAMESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Asturian; Bable", "ast", nullptr},
  {"Athapascan languages", "ath", nullptr},
  {"Australian languages", "aus", nullptr},
  {"Austronesian (Other)", "map", nullptr},
  {"Avaric", "ava", "av"},
  {"Avestan", "ave", "ae"},
  {"Awadhi", "awa", nullptr},
  {"Aymara", "aym", "ay"},
  {"Azerbaijani", "aze", "az", nullptr, MAKELCID( MAKELANGID(LANG_AZERI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Bable; Asturian", "ast", nullptr},
  {"Balinese", "ban", nullptr},
  {"Baltic (Other)", "bat", nullptr},
  {"Baluchi", "bal", nullptr},
  {"Bambara", "bam", "bm"},
  {"Bamileke languages", "bai", nullptr},
  {"Banda", "bad", nullptr},
  {"Bantu (Other)", "bnt", nullptr},
  {"Basa", "bas", nullptr},
  {"Bashkir", "bak", "ba", nullptr, MAKELCID( MAKELANGID(LANG_BASHKIR, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Basque", "baq", "eu", "eus", MAKELCID( MAKELANGID(LANG_BASQUE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Batak (Indonesia)", "btk", nullptr},
  {"Beja", "bej", nullptr},
  {"Belarusian", "bel", "be", nullptr, MAKELCID( MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Bemba", "bem", nullptr},
  {"Bengali", "ben", "bn", nullptr, MAKELCID( MAKELANGID(LANG_BENGALI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Berber (Other)", "ber", nullptr},
  {"Bhojpuri", "bho", nullptr},
  {"Bihari", "bih", "bh"},
  {"Bikol", "bik", nullptr},
  {"Bini", "bin", nullptr},
  {"Bislama", "bis", "bi"},
  {"Bokmål, Norwegian; Norwegian Bokmål", "nob", "nb"},
  {"Bosnian", "bos", "bs"},
  {"Braj", "bra", nullptr},
  {"Breton", "bre", "br", nullptr, MAKELCID( MAKELANGID(LANG_BRETON, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Buginese", "bug", nullptr},
  {"Bulgarian", "bul", "bg", nullptr, MAKELCID( MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Buriat", "bua", nullptr},
  {"Burmese", "bur", "my", "mya"},
  {"Caddo", "cad", nullptr},
  {"Carib", "car", nullptr},
  {"Spanish", "spa", "es", "esp", MAKELCID( MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Catalan", "cat", "ca", nullptr, MAKELCID( MAKELANGID(LANG_CATALAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Caucasian (Other)", "cau", nullptr},
  {"Cebuano", "ceb", nullptr},
  {"Celtic (Other)", "cel", nullptr},
  {"Central American Indian (Other)", "cai", nullptr},
  {"Chagatai", "chg", nullptr},
  {"Chamic languages", "cmc", nullptr},
  {"Chamorro", "cha", "ch"},
  {"Chechen", "che", "ce"},
  {"Cherokee", "chr", nullptr},
  {"Chewa; Chichewa; Nyanja", "nya", "ny"},
  {"Cheyenne", "chy", nullptr},
  {"Chibcha", "chb", nullptr},
  {"Chinese", "chi", "zh", "zho", MAKELCID( MAKELANGID(LANG_CHINESE, SUBLANG_NEUTRAL), SORT_DEFAULT)},
  {"Chinook jargon", "chn", nullptr},
  {"Chipewyan", "chp", nullptr},
  {"Choctaw", "cho", nullptr},
  {"Chuang; Zhuang", "zha", "za"},
  {"Church Slavic; Old Church Slavonic", "chu", "cu"},
  {"Old Church Slavonic; Old Slavonic; ", "chu", "cu"},
  {"Chuukese", "chk", nullptr},
  {"Chuvash", "chv", "cv"},
  {"Coptic", "cop", nullptr},
  {"Cornish", "cor", "kw"},
  {"Corsican", "cos", "co", nullptr, MAKELCID( MAKELANGID(LANG_CORSICAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Cree", "cre", "cr"},
  {"Creek", "mus", nullptr},
  {"Creoles and pidgins (Other)", "crp", nullptr},
  {"Creoles and pidgins,", "cpe", nullptr},
  //   {"English-based (Other)", nullptr, nullptr},
  {"Creoles and pidgins,", "cpf", nullptr},
  //   {"French-based (Other)", nullptr, nullptr},
  {"Creoles and pidgins,", "cpp", nullptr},
  //   {"Portuguese-based (Other)", nullptr, nullptr},
  {"Croatian", "scr", "hr", "hrv", MAKELCID( MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Cushitic (Other)", "cus", nullptr},
  {"Czech", "cze", "cs", "ces", MAKELCID( MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dakota", "dak", nullptr},
  {"Danish", "dan", "da", nullptr, MAKELCID( MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dargwa", "dar", nullptr},
  {"Dayak", "day", nullptr},
  {"Delaware", "del", nullptr},
  {"Dinka", "din", nullptr},
  {"Divehi", "div", "dv", nullptr, MAKELCID( MAKELANGID(LANG_DIVEHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dogri", "doi", nullptr},
  {"Dogrib", "dgr", nullptr},
  {"Dravidian (Other)", "dra", nullptr},
  {"Duala", "dua", nullptr},
  {"Dutch", "dut", "nl", "nld", MAKELCID( MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dutch, Middle (ca. 1050-1350)", "dum", nullptr},
  {"Dyula", "dyu", nullptr},
  {"Dzongkha", "dzo", "dz"},
  {"Efik", "efi", nullptr},
  {"Egyptian (Ancient)", "egy", nullptr},
  {"Ekajuk", "eka", nullptr},
  {"Elamite", "elx", nullptr},
  {"English", "eng", "en", nullptr, MAKELCID( MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"English, Middle (1100-1500)", "enm", nullptr},
  {"English, Old (ca.450-1100)", "ang", nullptr},
  {"Esperanto", "epo", "eo"},
  {"Estonian", "est", "et", nullptr, MAKELCID( MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ewe", "ewe", "ee"},
  {"Ewondo", "ewo", nullptr},
  {"Fang", "fan", nullptr},
  {"Fanti", "fat", nullptr},
  {"Faroese", "fao", "fo", nullptr, MAKELCID( MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Fijian", "fij", "fj"},
  {"Finnish", "fin", "fi", nullptr, MAKELCID( MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Finno-Ugrian (Other)", "fiu", nullptr},
  {"Flemish; Dutch", "dut", "nl"},
  {"Flemish; Dutch", "nld", "nl"},
  {"Fon", "fon", nullptr},
  {"French", "fre", "fr", "fra", MAKELCID( MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"French, Middle (ca.1400-1600)", "frm", nullptr},
  {"French, Old (842-ca.1400)", "fro", nullptr},
  {"Frisian", "fry", "fy", nullptr, MAKELCID( MAKELANGID(LANG_FRISIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Friulian", "fur", nullptr},
  {"Fulah", "ful", "ff"},
  {"Ga", "gaa", nullptr},
  {"Gaelic; Scottish Gaelic", "gla", "gd", nullptr, MAKELCID( MAKELANGID(LANG_GALICIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Gallegan", "glg", "gl"},
  {"Ganda", "lug", "lg"},
  {"Gayo", "gay", nullptr},
  {"Gbaya", "gba", nullptr},
  {"Geez", "gez", nullptr},
  {"Georgian", "geo", "ka", "kat", MAKELCID( MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"German", "ger", "de", "deu", MAKELCID( MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"German, Low; Saxon, Low; Low German; Low Saxon", "nds", nullptr},
  {"German, Middle High (ca.1050-1500)", "gmh", nullptr},
  {"German, Old High (ca.750-1050)", "goh", nullptr},
  {"Germanic (Other)", "gem", nullptr},
  {"Gikuyu; Kikuyu", "kik", "ki"},
  {"Gilbertese", "gil", nullptr},
  {"Gondi", "gon", nullptr},
  {"Gorontalo", "gor", nullptr},
  {"Gothic", "got", nullptr},
  {"Grebo", "grb", nullptr},
  {"Ancient Greek", "grc", nullptr},
  {"Greek", "gre", "el", "ell", MAKELCID( MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Greenlandic; Kalaallisut", "kal", "kl", nullptr, MAKELCID( MAKELANGID(LANG_GREENLANDIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Guarani", "grn", "gn"},
  {"Gujarati", "guj", "gu", nullptr, MAKELCID( MAKELANGID(LANG_GUJARATI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Gwich´in", "gwi", nullptr},
  {"Haida", "hai", nullptr},
  {"Hausa", "hau", "ha", nullptr, MAKELCID( MAKELANGID(LANG_HAUSA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Hawaiian", "haw", nullptr},
  {"Hebrew", "heb", "he", nullptr, MAKELCID( MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Herero", "her", "hz"},
  {"Hiligaynon", "hil", nullptr},
  {"Himachali", "him", nullptr},
  {"Hindi", "hin", "hi", nullptr, MAKELCID( MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Hiri Motu", "hmo", "ho"},
  {"Hittite", "hit", nullptr},
  {"Hmong", "hmn", nullptr},
  {"Hungarian", "hun", "hu", nullptr, MAKELCID( MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Hupa", "hup", nullptr},
  {"Iban", "iba", nullptr},
  {"Icelandic", "ice", "is", "isl", MAKELCID( MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ido", "ido", "io"},
  {"Igbo", "ibo", "ig", nullptr, MAKELCID( MAKELANGID(LANG_IGBO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ijo", "ijo", nullptr},
  {"Iloko", "ilo", nullptr},
  {"Inari Sami", "smn", nullptr},
  {"Indic (Other)", "inc", nullptr},
  {"Indo-European (Other)", "ine", nullptr},
  {"Indonesian", "ind", "id", nullptr, MAKELCID( MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ingush", "inh", nullptr},
  {"Interlingua (International", "ina", "ia"},
  //   {"Auxiliary Language Association)", nullptr, nullptr},
  {"Interlingue", "ile", "ie"},
  {"Inuktitut", "iku", "iu", nullptr, MAKELCID( MAKELANGID(LANG_INUKTITUT, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Inupiaq", "ipk", "ik"},
  {"Iranian (Other)", "ira", nullptr},
  {"Irish", "gle", "ga", nullptr, MAKELCID( MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Irish, Middle (900-1200)", "mga", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Irish, Old (to 900)", "sga", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Iroquoian languages", "iro", nullptr},
  {"Italian", "ita", "it", nullptr, MAKELCID( MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Japanese", "jpn", "ja", nullptr, MAKELCID( MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Javanese", "jav", "jv"},
  {"Judeo-Arabic", "jrb", nullptr},
  {"Judeo-Persian", "jpr", nullptr},
  {"Kabardian", "kbd", nullptr},
  {"Kabyle", "kab", nullptr},
  {"Kachin", "kac", nullptr},
  {"Kalaallisut; Greenlandic", "kal", "kl"},
  {"Kamba", "kam", nullptr},
  {"Kannada", "kan", "kn", nullptr, MAKELCID( MAKELANGID(LANG_KANNADA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kanuri", "kau", "kr"},
  {"Kara-Kalpak", "kaa", nullptr},
  {"Karen", "kar", nullptr},
  {"Kashmiri", "kas", "ks", nullptr, MAKELCID( MAKELANGID(LANG_KASHMIRI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kawi", "kaw", nullptr},
  {"Kazakh", "kaz", "kk", nullptr, MAKELCID( MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Khasi", "kha", nullptr},
  {"Khmer", "khm", "km", nullptr, MAKELCID( MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Khoisan (Other)", "khi", nullptr},
  {"Khotanese", "kho", nullptr},
  {"Kikuyu; Gikuyu", "kik", "ki"},
  {"Kimbundu", "kmb", nullptr},
  {"Kinyarwanda", "kin", "rw", nullptr, MAKELCID( MAKELANGID(LANG_KINYARWANDA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kirghiz", "kir", "ky"},
  {"Komi", "kom", "kv"},
  {"Kongo", "kon", "kg"},
  {"Konkani", "kok", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_KONKANI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Korean", "kor", "ko", nullptr, MAKELCID( MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kosraean", "kos", nullptr},
  {"Kpelle", "kpe", nullptr},
  {"Kru", "kro", nullptr},
  {"Kuanyama; Kwanyama", "kua", "kj"},
  {"Kumyk", "kum", nullptr},
  {"Kurdish", "kur", "ku"},
  {"Kurukh", "kru", nullptr},
  {"Kutenai", "kut", nullptr},
  {"Kwanyama, Kuanyama", "kua", "kj"},
  {"Ladino", "lad", nullptr},
  {"Lahnda", "lah", nullptr},
  {"Lamba", "lam", nullptr},
  {"Lao", "lao", "lo", nullptr, MAKELCID( MAKELANGID(LANG_LAO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Latin", "lat", "la"},
  {"Latvian", "lav", "lv", nullptr, MAKELCID( MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Letzeburgesch; Luxembourgish", "ltz", "lb"},
  {"Lezghian", "lez", nullptr},
  {"Limburgan; Limburger; Limburgish", "lim", "li"},
  {"Lingala", "lin", "ln"},
  {"Lithuanian", "lit", "lt", nullptr, MAKELCID( MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Lozi", "loz", nullptr},
  {"Luba-Katanga", "lub", "lu"},
  {"Luba-Lulua", "lua", nullptr},
  {"Luiseno", "lui", nullptr},
  {"Lule Sami", "smj", nullptr},
  {"Lunda", "lun", nullptr},
  {"Luo (Kenya and Tanzania)", "luo", nullptr},
  {"Lushai", "lus", nullptr},
  {"Luxembourgish; Letzeburgesch", "ltz", "lb", nullptr, MAKELCID( MAKELANGID(LANG_LUXEMBOURGISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Macedonian", "mac", "mk", "mkd", MAKELCID( MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Madurese", "mad", nullptr},
  {"Magahi", "mag", nullptr},
  {"Maithili", "mai", nullptr},
  {"Makasar", "mak", nullptr},
  {"Malagasy", "mlg", "mg"},
  {"Malay", "may", "ms", "msa", MAKELCID( MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Malayalam", "mal", "ml", nullptr, MAKELCID( MAKELANGID(LANG_MALAYALAM, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Maltese", "mlt", "mt", nullptr, MAKELCID( MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Manchu", "mnc", nullptr},
  {"Mandar", "mdr", nullptr},
  {"Mandingo", "man", nullptr},
  {"Manipuri", "mni", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_MANIPURI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Manobo languages", "mno", nullptr},
  {"Manx", "glv", "gv"},
  {"Maori", "mao", "mi", "mri", MAKELCID( MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Marathi", "mar", "mr", nullptr, MAKELCID( MAKELANGID(LANG_MARATHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Mari", "chm", nullptr},
  {"Marshallese", "mah", "mh"},
  {"Marwari", "mwr", nullptr},
  {"Masai", "mas", nullptr},
  {"Mayan languages", "myn", nullptr},
  {"Mende", "men", nullptr},
  {"Micmac", "mic", nullptr},
  {"Minangkabau", "min", nullptr},
  {"Miscellaneous languages", "mis", nullptr},
  {"Mohawk", "moh", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_MOHAWK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Moldavian", "mol", "mo"},
  {"Mon-Khmer (Other)", "mkh", nullptr},
  {"Mongo", "lol", nullptr},
  {"Mongolian", "mon", "mn", nullptr, MAKELCID( MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Mossi", "mos", nullptr},
  {"Multiple languages", "mul", nullptr},
  {"Munda languages", "mun", nullptr},
  {"Nahuatl", "nah", nullptr},
  {"Nauru", "nau", "na"},
  {"Navaho, Navajo", "nav", "nv"},
  {"Ndebele, North", "nde", "nd"},
  {"Ndebele, South", "nbl", "nr"},
  {"Ndonga", "ndo", "ng"},
  {"Neapolitan", "nap", nullptr},
  {"Nepali", "nep", "ne", nullptr, MAKELCID( MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Newari", "new", nullptr},
  {"Nias", "nia", nullptr},
  {"Niger-Kordofanian (Other)", "nic", nullptr},
  {"Nilo-Saharan (Other)", "ssa", nullptr},
  {"Niuean", "niu", nullptr},
  {"Norse, Old", "non", nullptr},
  {"North American Indian (Other)", "nai", nullptr},
  {"Northern Sami", "sme", "se"},
  {"North Ndebele", "nde", "nd"},
  {"Norwegian", "nor", "no", nullptr, MAKELCID( MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Norwegian Bokmål; Bokmål, Norwegian", "nob", "nb", nullptr, MAKELCID( MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Norwegian Nynorsk; Nynorsk, Norwegian", "nno", "nn", nullptr, MAKELCID( MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Nubian languages", "nub", nullptr},
  {"Nyamwezi", "nym", nullptr},
  {"Nyanja; Chichewa; Chewa", "nya", "ny"},
  {"Nyankole", "nyn", nullptr},
  {"Nynorsk, Norwegian; Norwegian Nynorsk", "nno", "nn"},
  {"Nyoro", "nyo", nullptr},
  {"Nzima", "nzi", nullptr},
  {"Occitan (post 1500},; Provençal", "oci", "oc", nullptr, MAKELCID( MAKELANGID(LANG_OCCITAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ojibwa", "oji", "oj"},
  {"Old Bulgarian; Old Slavonic; Church Slavonic;", "chu", "cu"},
  {"Oriya", "ori", "or"},
  {"Oromo", "orm", "om"},
  {"Osage", "osa", nullptr},
  {"Ossetian; Ossetic", "oss", "os"},
  {"Ossetic; Ossetian", "oss", "os"},
  {"Otomian languages", "oto", nullptr},
  {"Pahlavi", "pal", nullptr},
  {"Palauan", "pau", nullptr},
  {"Pali", "pli", "pi"},
  {"Pampanga", "pam", nullptr},
  {"Pangasinan", "pag", nullptr},
  {"Panjabi", "pan", "pa"},
  {"Papiamento", "pap", nullptr},
  {"Papuan (Other)", "paa", nullptr},
  {"Persian", "per", "fa", "fas", MAKELCID( MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Persian, Old (ca.600-400 B.C.)", "peo", nullptr},
  {"Philippine (Other)", "phi", nullptr},
  {"Phoenician", "phn", nullptr},
  {"Pohnpeian", "pon", nullptr},
  {"Polish", "pol", "pl", nullptr, MAKELCID( MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Portuguese", "por", "pt", nullptr, MAKELCID( MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Portuguese (Brazil)", "pob", "pb"},
  {"Prakrit languages", "pra", nullptr},
  {"Provençal; Occitan (post 1500)", "oci", "oc"},
  {"Provençal, Old (to 1500)", "pro", nullptr},
  {"Pushto", "pus", "ps"},
  {"Quechua", "que", "qu", nullptr, MAKELCID( MAKELANGID(LANG_QUECHUA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Raeto-Romance", "roh", "rm"},
  {"Rajasthani", "raj", nullptr},
  {"Rapanui", "rap", nullptr},
  {"Rarotongan", "rar", nullptr},
  {"Reserved for local use", "qaa-qtz", nullptr},
  {"Romance (Other)", "roa", nullptr},
  {"Romanian", "rum", "ro", "ron", MAKELCID( MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Romany", "rom", nullptr},
  {"Rundi", "run", "rn"},
  {"Russian", "rus", "ru", nullptr, MAKELCID( MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Salishan languages", "sal", nullptr},
  {"Samaritan Aramaic", "sam", nullptr},
  {"Sami languages (Other)", "smi", nullptr},
  {"Samoan", "smo", "sm"},
  {"Sandawe", "sad", nullptr},
  {"Sango", "sag", "sg"},
  {"Sanskrit", "san", "sa", nullptr, MAKELCID( MAKELANGID(LANG_SANSKRIT, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Santali", "sat", nullptr},
  {"Sardinian", "srd", "sc"},
  {"Sasak", "sas", nullptr},
  {"Scots", "sco", nullptr},
  {"Scottish Gaelic; Gaelic", "gla", "gd"},
  {"Selkup", "sel", nullptr},
  {"Semitic (Other)", "sem", nullptr},
  {"Serbian", "srp", "sr", "scc", MAKELCID( MAKELANGID(LANG_SERBIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Serer", "srr", nullptr},
  {"Shan", "shn", nullptr},
  {"Shona", "sna", "sn"},
  {"Sichuan Yi", "iii", "ii"},
  {"Sidamo", "sid", nullptr},
  {"Sign languages", "sgn", nullptr},
  {"Siksika", "bla", nullptr},
  {"Sindhi", "snd", "sd", nullptr, MAKELCID( MAKELANGID(LANG_SINDHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sinhalese", "sin", "si", nullptr, MAKELCID( MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sino-Tibetan (Other)", "sit", nullptr},
  {"Siouan languages", "sio", nullptr},
  {"Skolt Sami", "sms", nullptr},
  {"Slave (Athapascan)", "den", nullptr},
  {"Slavic (Other)", "sla", nullptr},
  {"Slovak", "slo", "sk", "slk", MAKELCID( MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Slovenian", "slv", "sl", nullptr, MAKELCID( MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sogdian", "sog", nullptr},
  {"Somali", "som", "so"},
  {"Songhai", "son", nullptr},
  {"Soninke", "snk", nullptr},
  {"Sorbian languages", "wen", nullptr},
  {"Sotho, Northern", "nso", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sotho, Southern", "sot", "st", nullptr, MAKELCID( MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"South American Indian (Other)", "sai", nullptr},
  {"Southern Sami", "sma", nullptr},
  {"South Ndebele", "nbl", "nr"},
  {"Sukuma", "suk", nullptr},
  {"Sumerian", "sux", nullptr},
  {"Sundanese", "sun", "su"},
  {"Susu", "sus", nullptr},
  {"Swahili", "swa", "sw", nullptr, MAKELCID( MAKELANGID(LANG_SWAHILI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Swati", "ssw", "ss"},
  {"Swedish", "swe", "sv", nullptr, MAKELCID( MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Syriac", "syr", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tagalog", "tgl", "tl"},
  {"Tahitian", "tah", "ty"},
  {"Tai (Other)", "tai", nullptr},
  {"Tajik", "tgk", "tg", nullptr, MAKELCID( MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tamashek", "tmh", nullptr},
  {"Tamil", "tam", "ta", nullptr, MAKELCID( MAKELANGID(LANG_TAMIL, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tatar", "tat", "tt", nullptr, MAKELCID( MAKELANGID(LANG_TATAR, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Telugu", "tel", "te",  nullptr, MAKELCID( MAKELANGID(LANG_TELUGU, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tereno", "ter", nullptr},
  {"Tetum", "tet", nullptr},
  {"Thai", "tha", "th", nullptr, MAKELCID( MAKELANGID(LANG_THAI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tibetan", "tib", "bo", "bod", MAKELCID( MAKELANGID(LANG_TIBETAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tigre", "tig", nullptr},
  {"Tigrinya", "tir", "ti", nullptr, MAKELCID( MAKELANGID(LANG_TIGRIGNA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Timne", "tem", nullptr},
  {"Tiv", "tiv", nullptr},
  {"Tlingit", "tli", nullptr},
  {"Tok Pisin", "tpi", nullptr},
  {"Tokelau", "tkl", nullptr},
  {"Tonga (Nyasa)", "tog", nullptr},
  {"Tonga (Tonga Islands)", "ton", "to"},
  {"Tsimshian", "tsi", nullptr},
  {"Tsonga", "tso", "ts"},
  {"Tswana", "tsn", "tn", nullptr, MAKELCID( MAKELANGID(LANG_TSWANA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tumbuka", "tum", nullptr},
  {"Tupi languages", "tup", nullptr},
  {"Turkish", "tur", "tr", nullptr, MAKELCID( MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Turkish, Ottoman (1500-1928)", "ota", nullptr,	nullptr, MAKELCID( MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Turkmen", "tuk", "tk", nullptr, MAKELCID( MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tuvalu", "tvl", nullptr},
  {"Tuvinian", "tyv", nullptr},
  {"Twi", "twi", "tw"},
  {"Ugaritic", "uga", nullptr},
  {"Uighur", "uig", "ug", nullptr, MAKELCID( MAKELANGID(LANG_UIGHUR, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ukrainian", "ukr", "uk", nullptr, MAKELCID( MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Umbundu", "umb", nullptr},
  {"Undetermined", "und", nullptr},
  {"Urdu", "urd", "ur", nullptr, MAKELCID( MAKELANGID(LANG_URDU, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Uzbek", "uzb", "uz", nullptr, MAKELCID( MAKELANGID(LANG_UZBEK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Vai", "vai", nullptr},
  {"Venda", "ven", "ve"},
  {"Vietnamese", "vie", "vi", nullptr, MAKELCID( MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Volapük", "vol", "vo"},
  {"Votic", "vot", nullptr},
  {"Wakashan languages", "wak", nullptr},
  {"Walamo", "wal", nullptr},
  {"Walloon", "wln", "wa"},
  {"Waray", "war", nullptr},
  {"Washo", "was", nullptr},
  {"Welsh", "wel", "cy", "cym", MAKELCID( MAKELANGID(LANG_WELSH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Wolof", "wol", "wo", nullptr, MAKELCID( MAKELANGID(LANG_WOLOF, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Xhosa", "xho", "xh", nullptr, MAKELCID( MAKELANGID(LANG_XHOSA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Yakut", "sah", nullptr, nullptr, MAKELCID( MAKELANGID(LANG_YAKUT, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Yao", "yao", nullptr},
  {"Yapese", "yap", nullptr},
  {"Yiddish", "yid", "yi"},
  {"Yoruba", "yor", "yo", nullptr, MAKELCID( MAKELANGID(LANG_YORUBA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Yupik languages", "ypk", nullptr},
  {"Zande", "znd", nullptr},
  {"Zapotec", "zap", nullptr},
  {"Zenaga", "zen", nullptr},
  {"Zhuang; Chuang", "zha", "za"},
  {"Zulu", "zul", "zu", nullptr, MAKELCID( MAKELANGID(LANG_ZULU, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Zuni", "zun", nullptr},
  {"Classical Newari", "nwc", nullptr},
  {"Klingon", "tlh", nullptr},
  {"Blin", "byn", nullptr},
  {"Lojban", "jbo", nullptr},
  {"Lower Sorbian", "dsb", nullptr},
  {"Upper Sorbian", "hsb", nullptr},
  {"Kashubian", "csb", nullptr},
  {"Crimean Turkish", "crh", nullptr},
  {"Erzya", "myv", nullptr},
  {"Moksha", "mdf", nullptr},
  {"Karachay-Balkar", "krc", nullptr},
  {"Adyghe", "ady", nullptr},
  {"Udmurt", "udm", nullptr},
  {"Dargwa", "dar", nullptr},
  {"Ingush", "inh", nullptr},
  {"Nogai", "nog", nullptr},
  {"Haitian", "hat", "ht"},
  {"Kalmyk", "xal", nullptr},
  {nullptr, nullptr, nullptr},
  {"No subtitles", "---", nullptr, nullptr, (LCID)LCID_NOSUBTITLES},
};

std::string ISO6391ToLanguage(LPCSTR code)
{
  CHAR tmp[2+1];
  strncpy_s(tmp, code, 2);
  tmp[2] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if(s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
    {
      std::string ret = std::string(s_isolangs[i].name);
      size_t i = ret.find(';');
      if(i != std::string::npos) {
        ret = ret.substr(0, i);
      }
      return ret;
    }
  }
  return std::string();
}

std::string ISO6392ToLanguage(LPCSTR code)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, code, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
    {
      std::string ret = std::string(s_isolangs[i].name);
      size_t i = ret.find(';');
      if(i != std::string::npos) {
        ret = ret.substr(0, i);
      }
      return ret;
    }
  }
  return std::string();
}

std::string ProbeLangForLanguage(LPCSTR code)
{
  if(strlen(code) == 3) {
    return ISO6392ToLanguage(code);
  } else if (strlen(code) >= 2) {
    return ISO6391ToLanguage(code);
  }
  return std::string();
}

static std::string ISO6392Check(LPCSTR lang)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, lang, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
    {
      return std::string(s_isolangs[i].iso6392);
    }
  }
  return std::string(tmp);
}

static std::string LanguageToISO6392(LPCSTR code)
{
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].name && !_stricmp(s_isolangs[i].name, code)))
    {
      return std::string(s_isolangs[i].iso6392);
    }
  }
  return std::string();
}

std::string ProbeForISO6392(LPCSTR lang)
{
  std::string isoLang;
  if (strlen(lang) == 2) {
    isoLang = ISO6391To6392(lang);
  } else if (strlen(lang) == 3) {
    isoLang = ISO6392Check(lang);
  } else if (strlen(lang) > 3) {
    isoLang = LanguageToISO6392(lang);
    if (isoLang.empty()) {
      std::regex ogmRegex("\\[([[:alpha:]]{3})\\]");
      std::cmatch res;
      bool found = std::regex_search(lang, res, ogmRegex);
      if (found && !res[1].str().empty()) {
        isoLang = ISO6392Check(res[1].str().c_str());
      }
    }
  }
  if (isoLang.empty())
    isoLang = std::string(lang);
  return isoLang;
}

LCID ISO6391ToLcid(LPCSTR code)
{
  CHAR tmp[2+1];
  strncpy_s(tmp, code, 2);
  tmp[2] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if(s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
    {
      return s_isolangs[i].lcid;
    }
  }
  return 0;
}

LCID ISO6392ToLcid(LPCSTR code)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, code, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
    {
      return s_isolangs[i].lcid;
    }
  }
  return 0;
}

std::string ISO6391To6392(LPCSTR code)
{
  CHAR tmp[2+1];
  strncpy_s(tmp, code, 2);
  tmp[2] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if(s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp)) {
      return s_isolangs[i].iso6392;
    }
  }
  return std::string(code);
}

std::string ISO6392To6391(LPCSTR code)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, code, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp))) {
      return s_isolangs[i].iso6391;
    }
  }
  return std::string();
}

LCID ProbeLangForLCID(LPCSTR code)
{
  if(strlen(code) == 3) {
    return ISO6392ToLcid(code);
  } else if (strlen(code) >= 2) {
    return ISO6391ToLcid(code);
  }
  return 0;
}
