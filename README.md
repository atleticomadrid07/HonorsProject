# README Document

## 1. PROJECT IDENTIFICATION

**PURPOSE:** Scrapes databases of scholarly works and compares text/abstract to user input to return the most relevant sources to the user’s query  
**AUTHORS:**  
Neel Rathi  
Dylan Lee  
Miqdad Chowdhury

---

## 2. HOW TO USE THE PROJECT

This section provides the necessary steps for users to install, configure, and run the project.

### PREREQUISITES
- GNU C++ Compiler (g++)

### INSTRUCTIONS
**STEP ONE:** Download the C++ file and place it into its own folder  
**STEP TWO:** Open the terminal and navigate to that folder/directory.  
**STEP THREE:** Compile the script using the command:  
`g++ -o relevant_link_filter.cpp -lcurl`  
**STEP FOUR:** Run the script using the command:  
`./honors_project`  
**STEP FIVE:** When prompted by the program, enter your scientific query and wait for the program to generate the top 15 most relevant links

---

## 3. PROGRAM LIMITATIONS

This section documents known limitations

### KNOWN LIMITATIONS/ERRORS

**LIMITATION 1:** Semantic Scholar API has a worldwide limit rate, 1000 requests per second, shared among all unauthenticated users. This may result in the program generating a ‘No articles found’ result. Rerun in 1-2 minutes to generate results.

**LIMITATION 2:** LLM API key has rate limits applied on the model used in the program: 30 requests per minute and 6000 tokens per minute. If this limit is exceeded, the program will not generate any results. Rerun in 1-2 minutes to generate results.

**LIMITATION 3:** Program only queries 45 articles and returns the top 15. If wanting a larger number of articles to scrape or a larger number of articles returned, go inside the program, and change lines 560 and lines 653 to change the number of articles scraped, or line 783 to return a different number than 15 articles.
