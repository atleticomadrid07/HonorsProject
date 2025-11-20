#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <map>

using namespace std;

// Your Groq API Key
//IMPORTANT: ADD API KEY WITH THIS LINK
//https://console.groq.com/keys
const string GROQ_API_KEY = "ENTER_KEY_HERE";
const string GROQ_API_URL = "https://api.groq.com/openai/v1/chat/completions";

// Semantic Scholar API
const string SEMANTIC_SCHOLAR_API_URL = "https://api.semanticscholar.org/graph/v1/paper/search";

// Structure to hold article information
struct Article {
    string title;
    int year;
    int citationCount;
    string abstract;
    string url;
    double relevancyScore;
};

// Callback function for libcurl to capture response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Function to escape JSON strings
string escapeJson(const string& input) {
    string output;
    for (char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += c; break;
        }
    }
    return output;
}

// Function to URL encode a string
string urlEncode(const string& str) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}

// Function to convert string to lowercase
string toLowercase(string str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// Function to tokenize text into words
vector<string> tokenize(const string& text) {
    vector<string> tokens;
    string word;
    string lowerText = toLowercase(text);
    
    for (char c : lowerText) {
        if (isalnum(c)) {
            word += c;
        } else if (!word.empty()) {
            tokens.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) {
        tokens.push_back(word);
    }
    
    return tokens;
}

// Function to calculate term frequency
map<string, double> calculateTF(const vector<string>& tokens) {
    map<string, double> tf;
    for (const string& token : tokens) {
        tf[token]++;
    }
    // Normalize by total number of tokens
    for (auto& pair : tf) {
        pair.second /= tokens.size();
    }
    return tf;
}

// Function to calculate cosine similarity between two texts
double calculateCosineSimilarity(const string& text1, const string& text2) {
    vector<string> tokens1 = tokenize(text1);
    vector<string> tokens2 = tokenize(text2);
    
    if (tokens1.empty() || tokens2.empty()) {
        return 0.0;
    }
    
    map<string, double> tf1 = calculateTF(tokens1);
    map<string, double> tf2 = calculateTF(tokens2);
    
    // Calculate dot product and magnitudes
    double dotProduct = 0.0;
    double magnitude1 = 0.0;
    double magnitude2 = 0.0;
    
    // Get all unique terms
    map<string, bool> allTerms;
    for (const auto& pair : tf1) allTerms[pair.first] = true;
    for (const auto& pair : tf2) allTerms[pair.first] = true;
    
    for (const auto& term : allTerms) {
        double val1 = tf1.count(term.first) ? tf1[term.first] : 0.0;
        double val2 = tf2.count(term.first) ? tf2[term.first] : 0.0;
        
        dotProduct += val1 * val2;
        magnitude1 += val1 * val1;
        magnitude2 += val2 * val2;
    }
    
    magnitude1 = sqrt(magnitude1);
    magnitude2 = sqrt(magnitude2);
    
    if (magnitude1 == 0.0 || magnitude2 == 0.0) {
        return 0.0;
    }
    
    return dotProduct / (magnitude1 * magnitude2);
}

// Function to calculate keyword match score
double calculateKeywordMatchScore(const Article& article, const string& originalKeywords, const string& expandedKeywords) {
    // Convert abstract to lowercase for matching
    string lowerAbstract = toLowercase(article.abstract);
    
    if (lowerAbstract.empty()) {
        return 0.0;
    }
    
    // Split original keywords (these are the important ones)
    vector<string> originalKeywordList;
    istringstream ss1(originalKeywords);
    string keyword;
    
    while (getline(ss1, keyword, ',')) {
        keyword.erase(0, keyword.find_first_not_of(" \t\n\r"));
        keyword.erase(keyword.find_last_not_of(" \t\n\r") + 1);
        if (!keyword.empty()) {
            originalKeywordList.push_back(toLowercase(keyword));
        }
    }
    
    // Split expanded keywords (these are supplementary)
    vector<string> expandedKeywordList;
    istringstream ss2(expandedKeywords);
    
    while (getline(ss2, keyword, ',')) {
        keyword.erase(0, keyword.find_first_not_of(" \t\n\r"));
        keyword.erase(keyword.find_last_not_of(" \t\n\r") + 1);
        if (!keyword.empty()) {
            string lowerKw = toLowercase(keyword);
            // Only add if it's NOT in the original list (these are the deconstructed words)
            if (find(originalKeywordList.begin(), originalKeywordList.end(), lowerKw) == originalKeywordList.end()) {
                expandedKeywordList.push_back(lowerKw);
            }
        }
    }
    
    if (originalKeywordList.empty()) {
        return 0.0;
    }
    
    // Count matches for original keywords (HIGH value)
    int originalMatches = 0;
    for (const string& kw : originalKeywordList) {
        if (lowerAbstract.find(kw) != string::npos) {
            originalMatches++;
        }
    }
    
    // Count matches for deconstructed keywords (LOW value)
    int expandedMatches = 0;
    for (const string& kw : expandedKeywordList) {
        if (lowerAbstract.find(kw) != string::npos) {
            expandedMatches++;
        }
    }
    
    // NEW: Bonus multiplier for matching MORE keywords
    // Base score calculation
    double originalScore = (static_cast<double>(originalMatches) / originalKeywordList.size()) * 100.0;
    
    double expandedScore = 0.0;
    if (!expandedKeywordList.empty()) {
        expandedScore = (static_cast<double>(expandedMatches) / expandedKeywordList.size()) * 20.0; // Max 20 points
    }
    
    // Combine base scores
    double baseScore = originalScore * 0.85 + expandedScore * 0.15;
    
    // Apply multiplier bonus for matching multiple original keywords
    // 1 keyword matched = 1.0x, 2 = 1.1x, 3 = 1.2x, 4+ = 1.3x
    double matchBonus = 1.0;
    if (originalMatches >= 4) {
        matchBonus = 1.3;
    } else if (originalMatches == 3) {
        matchBonus = 1.2;
    } else if (originalMatches == 2) {
        matchBonus = 1.1;
    }
    
    return min(100.0, baseScore * matchBonus);
}

// Function to calculate relevancy score
double calculateRelevancyScore(const Article& article, const string& query, const string& originalKeywords, const string& expandedKeywords, int currentYear) {
    // Adjusted weights to emphasize keyword matching
    const double KEYWORD_MATCH_WEIGHT = 0.35;  // Direct keyword matching
    const double COSINE_WEIGHT = 0.30;          // Reduced from 0.60
    const double RECENCY_WEIGHT = 0.20;         // Same
    const double CITATION_WEIGHT = 0.10;        // Reduced from 0.15
    const double LENGTH_WEIGHT = 0.05;          // Same
    
    // 1. Keyword Match Score (0-100)
    // Direct matching of keywords in abstract - prioritizes original keywords heavily
    double keywordScore = calculateKeywordMatchScore(article, originalKeywords, expandedKeywords);
    
    // 2. Cosine Similarity Score with curve (0-100)
    double cosineSimilarity = calculateCosineSimilarity(query, article.abstract);
    double curvedSimilarity = sqrt(cosineSimilarity);
    double cosineScore = min(100.0, curvedSimilarity * 120.0);
    
    // 3. Recency Score (0-100)
    // Papers from this year get 100, papers from 25 years ago get 0
    double recencyScore = 0.0;
    if (article.year > 0) {
        int yearsOld = currentYear - article.year;
        recencyScore = max(0.0, 100.0 - (yearsOld * 4.0)); // 4 points per year
    }
    
    // 4. Citation Score (0-100)
    // Using logarithmic scale, capped at 1000 citations, more generous
    double citationScore = 0.0;
    if (article.citationCount > 0) {
        int cappedCitations = min(article.citationCount, 1000);
        citationScore = (log(cappedCitations + 1) / log(101)) * 100.0;
        citationScore = min(100.0, citationScore);
    }
    
    // 5. Abstract Length Score (0-100)
    // Prefer abstracts between 100-1000 characters (more lenient)
    double lengthScore = 0.0;
    int abstractLen = article.abstract.length();
    if (abstractLen >= 100) {
        lengthScore = 100.0; // Any substantial abstract gets full points
    } else if (abstractLen > 0) {
        lengthScore = (abstractLen / 100.0) * 100.0;
    }
    
    // Calculate weighted total
    double totalScore = (keywordScore * KEYWORD_MATCH_WEIGHT) +
                       (cosineScore * COSINE_WEIGHT) +
                       (recencyScore * RECENCY_WEIGHT) +
                       (citationScore * CITATION_WEIGHT) +
                       (lengthScore * LENGTH_WEIGHT);
    
    return totalScore;
}

// Function to extract text from JSON response (Groq format)
string extractTextFromResponse(const string& response) {
    // Groq uses OpenAI format: {"choices":[{"message":{"content":"text here"}}]}
    size_t contentPos = response.find("\"content\"");
    if (contentPos == string::npos) {
        return "Error: Could not parse response";
    }
    
    // Find the content after "content": "
    size_t startQuote = response.find("\"", contentPos + 9);
    size_t endQuote = startQuote + 1;
    
    // Handle escaped quotes in the content
    while (endQuote < response.length()) {
        endQuote = response.find("\"", endQuote);
        if (endQuote == string::npos) {
            cout << "[DEBUG] Could not find closing quote" << endl;
            return "Error: Could not extract text from response";
        }
        // Check if this quote is escaped
        if (response[endQuote - 1] != '\\') {
            break;
        }
        endQuote++;
    }
    
    if (startQuote == string::npos || endQuote == string::npos) {
        cout << "[DEBUG] Could not extract text quotes" << endl;
        return "Error: Could not extract text from response";
    }
    
    string extractedText = response.substr(startQuote + 1, endQuote - startQuote - 1);
    
    // Clean up escape sequences
    size_t pos = 0;
    while ((pos = extractedText.find("\\n", pos)) != string::npos) {
        extractedText.replace(pos, 2, " ");
    }
    while ((pos = extractedText.find("\\r", pos)) != string::npos) {
        extractedText.replace(pos, 2, " ");
    }
    while ((pos = extractedText.find("\\\"", pos)) != string::npos) {
        extractedText.replace(pos, 2, "\"");
    }
    
    // Trim whitespace
    extractedText.erase(0, extractedText.find_first_not_of(" \t\n\r"));
    extractedText.erase(extractedText.find_last_not_of(" \t\n\r") + 1);
    
    return extractedText;
}

// Function to call Groq API
string callGroqAPI(const string& prompt) {
    CURL* curl;
    CURLcode res;
    string responseString;
    
    // Create JSON request body in OpenAI format
    // Using llama-3.1-70b-versatile model (fast and capable)
    string jsonData = "{"
                     "\"model\":\"llama-3.1-8b-instant\","
                     "\"messages\":[{\"role\":\"user\",\"content\":\"" + escapeJson(prompt) + "\"}],"
                     "\"temperature\":0.3,"
                     "\"max_tokens\":500"
                     "}";
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, GROQ_API_URL.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);
        
        // Set headers with API key
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        string authHeader = "Authorization: Bearer " + GROQ_API_KEY;
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return "Error: API call failed";
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        
        return extractTextFromResponse(responseString);
    }
    
    return "Error: Failed to initialize curl";
}

// Function to check if question is scientific using Groq
bool isScientificQuestion(const string& question) {
    string prompt = "You are an expert query classifier whose job is to differentiate\n"
                   "scientific queries/questions from general run-of-the-mill questions.\n\n"
                   "A \"Scientific\" query inquires about natural phenomena, technology, engineering, medicine, mathematics, or formal science. It often seeks to understand how or why something works.\n"
                   "It is the type of enquiry that requires support to strengthen its position, such as research and studies.\n\n"
                   "A \"Not_Scientific\" query asks for simple facts, directions, recipes, opinions, news, sports scores, or personal advice. These types of enquiries often are\n"
                   "quick and easy, not needed to be supported by any research or studies.\n\n"
                   "-- EXAMPLES --\n"
                   "Query: how do electromagnetic circuits work?\n"
                   "Classification: Scientific\n\n"
                   "Query: use of artificial intelligence in EVs\n"
                   "Classification: Scientific\n\n"
                   "Query: what is the weather today?\n"
                   "Classification: Not_Scientific\n\n"
                   "Query: how to cook an egg\n"
                   "Classification: Not_Scientific\n\n"
                   "Query: what were the football scores last night?\n"
                   "Classification: Not_Scientific\n"
                   "-- END EXAMPLES --\n\n"
                   "Now, classify the following query. Respond with ONLY one word: Scientific or Not_Scientific.\n\n"
                   "Query: " + question + "\n"
                   "Classification:";
    
    string response = callGroqAPI(prompt);
    
    if (response.find("Scientific") != string::npos) {
        if (response.find("Not_Scientific") != string::npos) {
            return false;
        }
        return true;
    }
    
    return false;
}

// Function to validate keywords and query using Groq
string validateQueryWithGroq(const string& question, const string& keywords) {
    string prompt = "You are an expert in scientific research and query validation.\n"
                   "Your task is to determine if a research question and its keywords are valid.\n\n"
                   "A query is INVALID if:\n"
                   "1. The keywords are completely nonsensical or unrelated to each other\n"
                   "2. The keywords don't match the query at all\n"
                   "3. The query is just a list of keywords/topics rather than an actual question or research statement\n"
                   "4. The concepts are contradictory or impossible (e.g., 'flat earth physics in spherical geometry')\n\n"
                   "A query is VALID if:\n"
                   "1. It's a proper question, statement, or research topic (not just keywords)\n"
                   "2. The keywords are coherent and scientifically related\n"
                   "3. The keywords match the intent of the query\n\n"
                   "Respond with ONLY one word: VALID or INVALID\n\n"
                   "-- EXAMPLES --\n"
                   "Query: What is the impact of artificial intelligence on healthcare diagnostics?\n"
                   "Keywords: artificial intelligence, healthcare diagnostics\n"
                   "Response: VALID\n\n"
                   "Query: How does quantum entanglement work in quantum computing?\n"
                   "Keywords: quantum entanglement, quantum computing\n"
                   "Response: VALID\n\n"
                   "Query: artificial intelligence, healthcare, technology\n"
                   "Keywords: artificial intelligence, healthcare, technology\n"
                   "Response: INVALID\n\n"
                   "Query: Purple elephants dancing with nuclear submarines\n"
                   "Keywords: purple elephants, nuclear submarines\n"
                   "Response: INVALID\n\n"
                   "Query: Impact of Shakespeare on quantum mechanics\n"
                   "Keywords: Shakespeare, quantum mechanics\n"
                   "Response: INVALID\n"
                   "-- END EXAMPLES --\n\n"
                   "Now, validate the following.\n\n"
                   "Query: " + question + "\n"
                   "Keywords: " + keywords + "\n"
                   "Response:";
    
    return callGroqAPI(prompt);
}

// Function to expand keywords (keep full phrases + split into individual words)
string expandKeywords(const string& keywords) {
    vector<string> expandedKeywords;
    istringstream ss(keywords);
    string keyword;
    
    // Split by commas to get individual keywords/phrases
    while (getline(ss, keyword, ',')) {
        // Trim whitespace
        keyword.erase(0, keyword.find_first_not_of(" \t\n\r"));
        keyword.erase(keyword.find_last_not_of(" \t\n\r") + 1);
        
        if (!keyword.empty()) {
            // Add the full keyword/phrase first
            expandedKeywords.push_back(keyword);
        }
    }
    
    // Now split each phrase into individual words
    ss.clear();
    ss.str(keywords);
    while (getline(ss, keyword, ',')) {
        // Trim whitespace
        keyword.erase(0, keyword.find_first_not_of(" \t\n\r"));
        keyword.erase(keyword.find_last_not_of(" \t\n\r") + 1);
        
        if (!keyword.empty()) {
            // Split multi-word phrases into individual words
            istringstream wordStream(keyword);
            string word;
            while (wordStream >> word) {
                // Remove punctuation from word
                word.erase(remove_if(word.begin(), word.end(), 
                    [](char c) { return !isalnum(c) && c != '-'; }), word.end());
                
                // Only add if it's not already in the list and not too short
                if (word.length() > 2 && 
                    find(expandedKeywords.begin(), expandedKeywords.end(), word) == expandedKeywords.end()) {
                    expandedKeywords.push_back(word);
                }
            }
        }
    }
    
    // Join back into comma-separated string
    string result;
    for (size_t i = 0; i < expandedKeywords.size(); i++) {
        result += expandedKeywords[i];
        if (i < expandedKeywords.size() - 1) {
            result += ", ";
        }
    }
    
    return result;
}

// Function to extract keywords using Groq
string extractKeywordsWithGroq(const string& question) {
    string prompt = "You are an expert in scientific research and natural language processing.\n"
                   "Your task is to extract the most important scientific and technical keywords or keyphrases\n"
                   "from a user's research question.\n\n"
                   "Focus on nouns, noun phrases, and technical terms that represent the core subjects of the query.\n"
                   "Ignore common stop words (e.g., 'the', 'is', 'a'), interrogative words (e.g., 'what', 'how'),\n"
                   "and vague verbs (e.g., 'affect', 'impact').\n\n"
                   "Return the keywords as a single, comma-separated string. Do not add any other explanation.\n\n"
                   "-- EXAMPLES --\n"
                   "Query: What is the effect of caffeine on human sleep cycles?\n"
                   "Keywords: caffeine, human sleep cycles\n\n"
                   "Query: The use of CRISPR-Cas9 for gene editing in treating genetic disorders.\n"
                   "Keywords: CRISPR-Cas9, gene editing, genetic disorders\n\n"
                   "Query: How do photovoltaic cells convert sunlight into electricity?\n"
                   "Keywords: photovoltaic cells, sunlight, electricity\n"
                   "-- END EXAMPLES --\n\n"
                   "Now, extract the keywords from the following query.\n\n"
                   "Query: " + question + "\n"
                   "Keywords:";
    
    return callGroqAPI(prompt);
}

// Function to parse Semantic Scholar results into Article structs
vector<Article> parseSemanticScholarResults(const string& jsonResponse) {
    vector<Article> articles;
    
    size_t dataPos = jsonResponse.find("\"data\"");
    if (dataPos == string::npos) {
        return articles;
    }
    
    size_t pos = dataPos;
    
    while (pos != string::npos && articles.size() < 45) {
        pos = jsonResponse.find("\"paperId\"", pos + 1);
        if (pos == string::npos) break;
        
        Article article;
        article.year = 0;
        article.citationCount = 0;
        article.relevancyScore = 0.0;
        
        // Extract title
        size_t titlePos = jsonResponse.find("\"title\"", pos);
        if (titlePos != string::npos) {
            size_t titleStart = jsonResponse.find("\"", titlePos + 7);
            size_t titleEnd = jsonResponse.find("\"", titleStart + 1);
            if (titleStart != string::npos && titleEnd != string::npos) {
                article.title = jsonResponse.substr(titleStart + 1, titleEnd - titleStart - 1);
            }
        }
        
        // Extract year
        size_t yearPos = jsonResponse.find("\"year\"", pos);
        if (yearPos != string::npos && yearPos < jsonResponse.find("\"paperId\"", pos + 1)) {
            size_t yearStart = jsonResponse.find(":", yearPos);
            size_t yearEnd = jsonResponse.find_first_of(",}", yearStart);
            if (yearStart != string::npos && yearEnd != string::npos) {
                string yearStr = jsonResponse.substr(yearStart + 1, yearEnd - yearStart - 1);
                yearStr.erase(remove_if(yearStr.begin(), yearStr.end(), 
                    [](char c) { return isspace(c) || c == '\0'; }), yearStr.end());
                if (yearStr != "null" && !yearStr.empty()) {
                    article.year = stoi(yearStr);
                }
            }
        }
        
        // Extract citation count
        size_t citationPos = jsonResponse.find("\"citationCount\"", pos);
        if (citationPos != string::npos && citationPos < jsonResponse.find("\"paperId\"", pos + 1)) {
            size_t citationStart = jsonResponse.find(":", citationPos);
            size_t citationEnd = jsonResponse.find_first_of(",}", citationStart);
            if (citationStart != string::npos && citationEnd != string::npos) {
                string citationsStr = jsonResponse.substr(citationStart + 1, citationEnd - citationStart - 1);
                citationsStr.erase(remove_if(citationsStr.begin(), citationsStr.end(), 
                    [](char c) { return isspace(c) || c == '\0'; }), citationsStr.end());
                if (citationsStr != "null" && !citationsStr.empty()) {
                    article.citationCount = stoi(citationsStr);
                }
            }
        }
        
        // Extract abstract
        size_t abstractPos = jsonResponse.find("\"abstract\"", pos);
        if (abstractPos != string::npos && abstractPos < jsonResponse.find("\"paperId\"", pos + 1)) {
            size_t abstractStart = jsonResponse.find("\"", abstractPos + 10);
            size_t abstractEnd = jsonResponse.find("\"", abstractStart + 1);
            if (abstractStart != string::npos && abstractEnd != string::npos) {
                article.abstract = jsonResponse.substr(abstractStart + 1, abstractEnd - abstractStart - 1);
                if (article.abstract == "null") {
                    article.abstract = "";
                }
            }
        }
        
        // Extract URL
        size_t urlPos = jsonResponse.find("\"url\"", pos);
        if (urlPos != string::npos && urlPos < jsonResponse.find("\"paperId\"", pos + 1)) {
            size_t urlStart = jsonResponse.find("\"", urlPos + 5);
            size_t urlEnd = jsonResponse.find("\"", urlStart + 1);
            if (urlStart != string::npos && urlEnd != string::npos) {
                article.url = jsonResponse.substr(urlStart + 1, urlEnd - urlStart - 1);
            }
        }
        
        articles.push_back(article);
    }
    
    return articles;
}

// Function to search Semantic Scholar and return articles
vector<Article> searchSemanticScholar(const string& keywords) {
    CURL* curl;
    CURLcode res;
    string responseString;
    vector<Article> articles;
    
    time_t now = time(0);
    tm* ltm = localtime(&now);
    int currentYear = 1900 + ltm->tm_year;
    int startYear = currentYear - 25;
    
    string query = urlEncode(keywords);
    string url = SEMANTIC_SCHOLAR_API_URL + "?query=" + query + 
                 "&year=" + to_string(startYear) + "-" + to_string(currentYear) +
                 "&limit=45" +
                 "&fields=title,year,abstract,citationCount,url";
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "ScientificResearchApp/1.0");
        
        res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            return articles;
        }
        
        curl_easy_cleanup(curl);
        
        articles = parseSemanticScholarResults(responseString);
    }
    
    return articles;
}

// Function to display articles with relevancy scores
void displayRankedArticles(const vector<Article>& articles) {
    if (articles.empty()) {
        cout << "\nNo articles found for the given keywords." << endl;
        return;
    }
    
    for (size_t i = 0; i < articles.size(); i++) {
        cout << "\n=== Article " << (i + 1) << " ===" << endl;
        cout << "Relevancy Score: " << fixed << setprecision(2) << articles[i].relevancyScore << "%" << endl;
        cout << "Title: " << articles[i].title << endl;
        cout << "Year: " << (articles[i].year > 0 ? to_string(articles[i].year) : "N/A") << endl;
        cout << "Citations: " << articles[i].citationCount << endl;
        
        if (!articles[i].abstract.empty()) {
            string abstract = articles[i].abstract;
            if (abstract.length() > 300) {
                abstract = abstract.substr(0, 297) + "...";
            }
            cout << "Abstract: " << abstract << endl;
        } else {
            cout << "Abstract: N/A" << endl;
        }
        
        cout << "URL: " << articles[i].url << endl;
    }
    
    cout << "\n--- Total articles found: " << articles.size() << " ---" << endl;
}

int main() {
    string question;
    
    cout << "=== Scientific Question Identifier ===" << endl;
    cout << "\nStep 1: Identifying the Scope\n" << endl;
    
    cout << "Enter your question: ";
    getline(cin, question);
    
    // Step 1: Determine if it's a scientific question using Groq
    cout << "\n--- Step 1: Classification ---" << endl;
    cout << "Calling Groq API to classify question..." << endl;
    
    bool isScientific = isScientificQuestion(question);
    
    cout << "Is this a scientific question? " << (isScientific ? "TRUE" : "FALSE") << endl;
    
    if (isScientific) {
        // Step 2: Extract keywords
        cout << "\n--- Step 2: Extracting Keywords ---" << endl;
        cout << "Calling Groq API to extract keywords..." << endl;
        
        string keywords = extractKeywordsWithGroq(question);
        
        cout << "\nExtracted Keywords: " << keywords << endl;
        
        // Validate query and keywords using Groq
        cout << "Validating query..." << endl;
        string validationResult = validateQueryWithGroq(question, keywords);
        
        // Check if query is invalid
        if (validationResult.find("INVALID") != string::npos) {
            cout << "\n[ERROR] The query is invalid for one of the following reasons:" << endl;
            cout << "  - Keywords are unrelated or nonsensical" << endl;
            cout << "  - Query is just a list of keywords, not an actual question or research topic" << endl;
            cout << "  - Concepts are contradictory or don't make sense together" << endl;
            cout << "\nPlease rephrase as a proper scientific question or research statement." << endl;
            return 0;
        }
        
        cout << "Query validated successfully!" << endl;
        
        // Expand keywords to include individual words
        string expandedKeywords = expandKeywords(keywords);
        cout << "Expanded Keywords: " << expandedKeywords << endl;
        
        // Step 3: Search Semantic Scholar
        cout << "\n--- Step 3: Searching Semantic Scholar ---" << endl;
        cout << "Searching for articles..." << endl;
        
        vector<Article> articles = searchSemanticScholar(expandedKeywords);
        
        if (!articles.empty()) {
            // Step 4: Score and rank articles
            cout << "\n--- Step 4: Scoring and Ranking Articles ---" << endl;
            cout << "Calculating relevancy scores..." << endl;
            
            time_t now = time(0);
            tm* ltm = localtime(&now);
            int currentYear = 1900 + ltm->tm_year;
            
            // Calculate relevancy scores
            for (auto& article : articles) {
                article.relevancyScore = calculateRelevancyScore(article, question, keywords, expandedKeywords, currentYear);
            }
            
            // Sort by relevancy score (descending)
            sort(articles.begin(), articles.end(), 
                [](const Article& a, const Article& b) {
                    return a.relevancyScore > b.relevancyScore;
                });
            
            // Filter to top 15 articles
            vector<Article> top15Articles;
            size_t displayCount = min(articles.size(), (size_t)15);
            for (size_t i = 0; i < displayCount; i++) {
                top15Articles.push_back(articles[i]);
            }
            
            // Display top 15 articles
            cout << "\n--- Ranked Results (Top " << top15Articles.size() << " of " << articles.size() << " Articles) ---" << endl;
            displayRankedArticles(top15Articles);
        } else {
            cout << "\nNo articles found for the given keywords." << endl;
        }
    } else {
        cout << "\nQuestion is not scientific. Skipping keyword extraction and article search." << endl;
    }
    
    return 0;
}
