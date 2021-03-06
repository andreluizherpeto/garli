// GARLI version 0.96b8 source code
// Copyright 2005-2008 Derrick J. Zwickl
// email: zwickl@nescent.org
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

//  This file was adapted from from the BasicCmdLine example provided as
//  part of the NCL

//	Copyright (C) 1999-2002 Paul O. Lewis
//
//	This file is part of NCL (Nexus Class Library).
//
//	NCL is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	NCL is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with NCL; if not, write to the Free Software Foundation, Inc., 
//	59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//

#include "defs.h"
#include "ncl.h"
#include "garlireader.h"
#include "outputman.h"
#include "errorexception.h"
#include "model.h"
#include <sstream>

int GARLI_main( int argc, char* argv[] );

extern OutputManager outman;

/*----------------------------------------------------------------------------------------------------------------------
|	The constructor simply passes along `i' to the base class constructor. Nothing else needs to be done.
*/
MyNexusToken::MyNexusToken(
  istream &i)	/* the input file stream attached to the NEXUS file to be read */
  : NxsToken(i)
	{
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Overrides the NxsToken::OutputComment virtual function (which does nothing) to display output comments [!comments
|	like this one beginning with an exclamation point]. The output comment is passed through the OutputManager
*/
void MyNexusToken::OutputComment(
  const NxsString &msg)	/* the output comment to be displayed */
	{
	unsigned pos;
	string s;
	//changing this again - just eating the Garli output comments
	s = "GarliScore";
	pos = msg.find(s);
	if(pos != string::npos){
		//outman.UserMessage("This is apparently a tree inferred by Garli in a previous run.  Its score was %s", msg.substr(s.length()).c_str());
		return;
		}
	s = "GarliModel";
	pos = msg.find(s);
	if(pos != string::npos){
		//outman.UserMessage("Garli's model parameter values used in inferring this tree:\n\t%s", msg.substr(s.length()).c_str());
		return;
		}
	
	s =	"****NOTE";//this is a note about the parameter values either being from a run that was terimated early or that 
					//they are only optimal for a certain tree. This is mainly for output when reading the trees in PAUP
					//and we will just ignore them here
	pos = msg.find(s);
	if(pos != string::npos) return;
	
	outman.UserMessage("\nCOMMENT FOUND IN NEXUS FILE (output verbatim):");
	outman.UserMessage(msg);
	outman.UserMessage("(END OF NEXUS COMMENT)");
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Initializes the `id' data member to "GarliReader" and calls the FactoryDefaults member function to perform the 
|	remaining initializations. The data member `'' is set to NULL so that memory will be allocated for it in
|	FactoryDefaults.
*/
GarliReader::GarliReader()
	{
	id				= "GARLI";

	// Make sure all data members that are pointers are initialized to NULL!
	// Failure to do this will result in problems because functions such as
	// FactoryDefaults() will try to delete an object if it is non-NULL.
	//
	taxa			= NULL;
	trees			= NULL;
	assumptions		= NULL;
	distances		= NULL;
	characters		= NULL;
	data			= NULL;
	next_command	= NULL;

	FactoryDefaults();
	
	//moved all of this allocation to FactoryDefaults
	/*
	taxa			= new NxsTaxaBlock();
	trees			= new NxsTreesBlock(taxa);
#if defined(NCL_MAJOR_VERSION) && (NCL_MAJOR_VERSION >= 2) && (NCL_MINOR_VERSION >= 1)
	trees->SetAllowImplicitNames(true);
#endif
	assumptions		= new NxsAssumptionsBlock(taxa);
	characters		= new NxsCharactersBlock(taxa, assumptions);
	distances		= new NxsDistancesBlock(taxa);
	data			= new NxsDataBlock(taxa, assumptions);

	Add(taxa);
	Add(trees);
	Add(assumptions);
	Add(characters);
	Add(distances);
	Add(data);
	Add(this);
*/
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Closes `logf' if it is open and deletes memory allocated to `next_command'.
*/
GarliReader::~GarliReader()
	{
	assert(next_command != NULL);
	delete [] next_command;

	if (logf_open)
		logf.close();
	}

/*----------------------------------------------------------------------------------------------------------------------
|	The code here is identical to the base class version (simply returns 0), so the code here should either be 
|	modified or this derived version eliminated altogether. Under what circumstances would you need to modify the 
|	default code, you ask? This function should be modified to something meaningful if this derived class needs to 
|	construct and run a NxsSetReader object to read a set involving characters. The NxsSetReader object may need to 
|	use this function to look up a character label encountered in the set. A class that overrides this method should 
|	return the character index in the range [1..`nchar']; i.e., add one to the 0-offset index.
*/
unsigned GarliReader::CharLabelToNumber(
  NxsString)	/* the character label to be translated to character number */
	{
	return 0;
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called by the NxsReader object when a block named `blockName' is entered. Allows program to notify user of 
|	progress in parsing the NEXUS file. Also gives program the opportunity to ask user if it is ok to purge data 
|	currently contained in this block. If user is asked whether existing data should be deleted, and the answer comes 
|	back no, then then return false, otherwise return true. Overrides pure virtual function in class NxsReader. 
*/
bool GarliReader::EnteringBlock(
  NxsString blockName)	/* the name of the block just entered */
	{
	message = "Reading ";
	message += blockName;
	message += " block...";
	PrintMessage(false);
#ifndef FACTORY
		
	if(blockName.Equals("CHARACTERS")){
		if(characters->IsEmpty() == false){
			charBlocks.push_back(characters);
			NxsCharactersBlock *newchar	= new NxsCharactersBlock(taxa, assumptions);
			Reassign(characters, newchar);
			characters = newchar;
			currBlock = characters;
			}
		}
	else if(blockName.Equals("ASSUMPTIONS") && charBlocks.empty() == false){
		outman.UserMessage("\n\nWARNING:Assumptions block found with multiple characters blocks.\n\tCheck output carefully to ensure that any excluded characters\n\twere applied to the correct characters block.\n\tThe correct exclusion format in an Assumptions block is this:\n\tEXSET * UNTITLED = 10-11;\n\tBut it will only be applied to the LAST characters block in the file.\n\tYou may want to comment out unused characters blocks.\n");
		}
#endif
	//3/25/08 if we already found a Garli block with a model (e.g. in the dataset file)
	//we should crap out, since we don't know which one the user meant to use
	//this is a change from previous behavior, in which I wanted the second to just override the first.
	if(blockName.Equals("GARLI") && FoundModelString())
		throw ErrorException("Multiple GARLI blocks found (possibly in multiple files).\n\tRemove or comment out all but one.");

	return true;
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called by the NxsReader object when exiting a block named `blockName'. Allows program to notify user of progress 
|	in parsing the NEXUS file. Virtual function that overrides the pure virtual function in the base class NxsReader.
*/
void GarliReader::ExitingBlock(
  NxsString blockName)	/* the name of the block just exited */
	{
	//message to indicate that we sucessfully read whatever block this was
	string mess;
	if(blockName.Equals("CHARACTERS")){
		switch (static_cast<NxsCharactersBlock *>(currBlock)->GetDataType()){
			case NxsCharactersBlock::dna:
				mess = " found dna data...";
				break;
			case NxsCharactersBlock::rna:
				mess = " found rna data...";
				break;
			case NxsCharactersBlock::protein:
				mess = " found protein data...";
				break;
			case NxsCharactersBlock::standard:
				mess = " found standard data...";
				break;
			case NxsCharactersBlock::nucleotide:
				mess = " found nucleotide data...";
				break;
			case NxsCharactersBlock::continuous:
				mess = " found continuous data...";
				break;
			}
		}

	mess += " successful";
	outman.UserMessage(mess);
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Sets all data members to their factory default settings: `inf_open', `logf_open' and `quit_now' are set to false; 
|	`message' to the null string, and the pointers `data', `characters', `assumptions', `taxa' and `trees' 
|	are all set to NULL. The C-string `next_command' is allocated COMMAND_MAXLEN + 1 bytes if it is currently NULL, 
|	and its first byte is set to the null character to create an empty `next_command' string.
*/
void GarliReader::FactoryDefaults()
	{
	isEmpty = true;
	inf_open = false;
	logf_open = false;
	quit_now = false;
	message.clear();

	if (trees != NULL)
		{
		Detach(trees);
		delete trees;
		trees = NULL;
		}

	if (taxa != NULL)
		{
		Detach(taxa);
		delete taxa;
		taxa = NULL;
		}

	if (assumptions != NULL)
		{
		Detach(assumptions);
		delete assumptions;
		assumptions = NULL;
		}

	if (distances != NULL)
		{
		Detach(distances);
		delete distances;
		distances = NULL;
		}

	if (characters != NULL)
		{
		Detach(characters);
		delete characters;
		characters = NULL;
		}

	if (charBlocks.size() > 0)
		{
		for(vector<NxsCharactersBlock*>::iterator it = charBlocks.begin();it != charBlocks.end(); it++){
//			Detach(*it);
			delete *it;
//			charBlocks.erase(it);
			}
		charBlocks.clear();
		}
	if (data != NULL)
		{
		Detach(data);
		delete data;
		data = NULL;
		}
	//"this" (the garli reader itself) is added to the block list to provide support for Garli blocks.
	//we can't delete it, but need to detach it
	Detach(this);

	if (next_command == NULL)
		next_command = new char[COMMAND_MAXLEN + 1];
	next_command[0] = '\0';
		
#ifdef FACTORY 
		//not sure what will need to be done here in the factory case
		/*
		//moved all of this allocation to here from the constructor - otherwise calling FactoryDefaults
		//just makes the GarliReader empty
		taxa			= new NxsTaxaBlock();
		trees			= new NxsTreesBlock(taxa);
#if defined(NCL_MAJOR_VERSION) && (NCL_MAJOR_VERSION >= 2) && (NCL_MINOR_VERSION >= 1)
		trees->SetAllowImplicitNames(true);
#endif
		assumptions		= new NxsAssumptionsBlock(taxa);
		characters		= new NxsCharactersBlock(taxa, assumptions);
		distances		= new NxsDistancesBlock(taxa);
		data			= new NxsDataBlock(taxa, assumptions);
		
		Add(taxa);
		Add(trees);
		Add(assumptions);
		Add(characters);
		Add(distances);
		Add(data);
		Add(this);		
*/
 #else
	//moved all of this allocation to here from the constructor - otherwise calling FactoryDefaults
	//just makes the GarliReader empty
	taxa			= new NxsTaxaBlock();
	trees			= new NxsTreesBlock(taxa);
#if defined(NCL_MAJOR_VERSION) && (NCL_MAJOR_VERSION >= 2) && (NCL_MINOR_VERSION >= 1)
	trees->SetAllowImplicitNames(true);
#endif
	assumptions		= new NxsAssumptionsBlock(taxa);
	characters		= new NxsCharactersBlock(taxa, assumptions);
	distances		= new NxsDistancesBlock(taxa);
	data			= new NxsDataBlock(taxa, assumptions);

	Add(taxa);
	Add(trees);
	Add(assumptions);
	Add(characters);
	Add(distances);
	Add(data);
	Add(this);
#endif
		
	modelString = "";
	}

	//DJZ this is my function, replacing an old one that appeared in funcs.cpp
	//simpler now, since it uses NxsMultiFormatReader
bool GarliReader::ReadData(const char* filename, const ModelSpecification &modspec){
	//first use a few of my crappy functions to try to diagnose the type of file and data
	//then call the NxsMultiFormatReader functions to process it

	try{
		if (!FileExists(filename))	{
			throw ErrorException("data file not found: %s!", filename);
			}
		//if it is Nexus, don't need to specify anything else in advance
		if(FileIsNexus(filename)){
			outman.UserMessage("Attempting to read data file in Nexus format (using NCL):\n\t%s ...", filename);
			ReadFilepath(filename, NEXUS_FORMAT);
			}
		else if(FileIsFasta(filename)){
			if(modspec.IsAminoAcid()){
				outman.UserMessage("Attempting to read data file as Fasta amino acid sequence (using NCL):\n\t%s ...", filename);
				ReadFilepath(filename, FASTA_AA_FORMAT);
				}
			else{ //DEBUG not sure how this will behave in the case of RNA
				outman.UserMessage("Attempting to read data file as Fasta DNA sequence (using NCL):\n\t%s ...", filename);
				ReadFilepath(filename, FASTA_DNA_FORMAT);
				}
			}
		else{//DEBUG assuming that otherwise the file is phylip format, and for the moment with relaxed names and non-interleaved
			if(modspec.IsAminoAcid()){
				outman.UserMessage("Attempting to read data file as Phylip amino acid sequence (using NCL):\n\t%s ...", filename);
				ReadFilepath(filename, RELAXED_PHYLIP_AA_FORMAT);
				}
			else{
				outman.UserMessage("Attempting to read data file as Phylip dna sequence (using NCL):\n\t%s ...", filename);
				ReadFilepath(filename, RELAXED_PHYLIP_DNA_FORMAT);
				}
			}
		return true;
		}
	catch(NxsException &err){
		NexusError(err.msg, err.pos, err.line, err.col);
		throw ErrorException("NCL encountered a problem reading the dataset.");
		}
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Returns true if file named `fn' already exists, false otherwise.
*/
bool GarliReader::FileExists(
  const char *fn) const	/* the name of the file to check */
	{
	bool exists = false;

	FILE *fp = fopen(fn, "r");
	if (fp != NULL)
		{
		fclose(fp);
		exists = true;
		}

	return exists;
	}

//DJZ there are my crappy functions to try to diagnose file type
bool GarliReader::FileIsNexus(const char *name) const{
	if (!FileExists(name))	{
		throw ErrorException("could not open file: %s!", name);
		}

	bool nexus = false;
	FILE *inf;
#ifdef BOINC
	inf = boinc_fopen(name, "r");
#else
	inf = fopen(name, "r");
#endif
	char buf[1024];
	GetToken(inf, buf, 1024);
	if(!(_stricmp(buf, "#NEXUS"))) nexus = true;

	fclose(inf);
	return nexus;
	}

bool GarliReader::FileIsFasta(const char *name) const{
	if (!FileExists(name))	{
		throw ErrorException("could not open file: %s!", name);
		}

	bool fasta = false;
	FILE *inf;
#ifdef BOINC
	inf = boinc_fopen(name, "r");
#else
	inf = fopen(name, "r");
#endif
	char buf[1024];
	GetToken(inf, buf, 1024);
	if(buf[0] == '>') fasta = true;

	fclose(inf);
	return fasta;
	}

int GarliReader::GetToken( FILE *in, char* tokenbuf, int maxlen) const{
	int ok = 1;

	int i;
	char ch = ' ';

	// skip leading whitespace
	while( in && ( isspace(ch) || ch == '[' ) ){
		ch = getc(in);
		}
	if( !in ) return 0;

	tokenbuf[0] = ch;
	tokenbuf[1] = '\0';
	tokenbuf[maxlen-1] = '\0';
		
	for( i = 1; i < maxlen-1; i++ ) {
		ch = getc(in);
		if( isspace(ch) || ch == ']' )
			break;
		tokenbuf[i] = ch;
		tokenbuf[i+1] = '\0';
	}

	if( i >= maxlen-1 )
		ok = 0;

	return ok;
}


/*----------------------------------------------------------------------------------------------------------------------
|	Called whenever a file name needs to be read from either the command line or a file. Expects next token to be "=" 
|	followed by the token representing the file name. Call this function after, say, the keyword "file" has been read 
|	in the following LOG command:
|>
|	log file=doofus.txt start replace;
|>
|	Note that this function will read only the "=doofus.txt " leaving "start replace;" in the stream for reading at 
|	a later time.
*/
NxsString GarliReader::GetFileName(
  NxsToken &token)	/* the token used to read from `in' */
	{
	// Eat the equals sign
	//
	token.GetNextToken();

	if (!token.Equals("="))
		{
		errormsg = "Expecting an equals sign, but found ";
		errormsg += token.GetToken();
		errormsg += " instead";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	// Now get the filename itself
	//
	token.GetNextToken();

	return token.GetToken();
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called when the END or ENDBLOCK command needs to be parsed from within the GarliReader block. Basically just 
|	checks to make sure the next token in the data file is a semicolon.
*/
void GarliReader::HandleEndblock(
  NxsToken &token)	/*  the token used to read from `in' */
	{
	// Get the semicolon following END or ENDBLOCK token
	//
	token.GetNextToken();

	if (!token.Equals(";"))
		{
		errormsg = "Expecting ';' to terminate the END or ENDBLOCK command, but found ";
		errormsg += token.GetToken();
		errormsg += " instead";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Handles everything after the EXECUTE keyword and the terminating semicolon. Purges all blocks before executing 
|	file specified, and no warning is given of this.
| DJZ THIS IS NOT THE VERSION OF HandleExecute USED.  See the other overloaded version below.
*/
void GarliReader::HandleExecute(
  NxsToken &token)	/* the token used to read from `in' */
	{
	// Issuing the EXECUTE command from within a file is a no-no (at least in this program)
	//
	if (inf_open)
		{
		errormsg = "Cannot issue execute command from within a GarliReader block";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	// Get the file name to execute (note: if filename contains underscores, these will be
	// automatically converted to spaces; user should surround such filenames with single quotes)
	//
	token.GetNextToken();

	NxsString fn = token.GetToken();

	// Get the semicolon terminating the EXECUTE command
	//
	token.GetNextToken();

	if (!token.Equals(";"))
		{
		errormsg = "Expecting ';' to terminate the EXECUTE command, but found ";
		errormsg += token.GetToken();
		errormsg += " instead";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	if (FileExists(fn.c_str()))
		{
		cerr << endl;
		cerr << "Opening " << fn << "..." << endl;

		PurgeBlocks();

		ifstream inf(fn.c_str(), ios::binary | ios::in);

		inf_open = true;

		MyNexusToken ftoken(inf);

		try
			{
			Execute(ftoken);
			}
		catch(NxsException x)
			{
			NexusError(errormsg, x.pos, x.line, x.col);
			}

		if (inf_open)
			inf.close();
		inf_open = false;

		// Users are allowed to put DATA blocks in their NEXUS files, but internally the data is always
		// stored in a NxsCharacterBlock object.
		//
		if (characters->IsEmpty() && !data->IsEmpty())
			{
			data->TransferTo(*characters);
			}

		}	// if (FileExists(fn.c_str()))

	else
		{
		cerr << endl;
		cerr << "Oops! Could not find specified file: " << fn << endl;
		}
	}

int GarliReader::HandleExecute(const char *filename, bool purge)	
	{
	// The filename to execute is passed in
	//

	NxsString fn = filename;
	int ret = 0;

	if (FileExists(fn.c_str()))
		{
#ifndef FACTORY
		if(purge) PurgeBlocks();
#endif
			
		ifstream inf(fn.c_str(), ios::binary | ios::in);

		inf_open = true;

		MyNexusToken ftoken(inf);

		try{
			Execute(ftoken);
			}
		catch(NxsException x){
			//DJZ 3/24/08 this was a bug that I inherited from the NCL example BasicCmdLine
			//the actual error message in x.msg was never getting printed because the empty
			//errormsg member of NexusBlock was being passed instead of the error stored in the
			//NxsException
			//NexusError(errormsg, x.pos, x.line, x.col);
			NexusError(x.msg, x.pos, x.line, x.col);
			ret = 1;//error
			}

		if (inf_open)
			inf.close();
		inf_open = false;

		// Users are allowed to put DATA blocks in their NEXUS files, but internally the data is always
		// stored in a NxsCharacterBlock object.
		//
		if (characters->IsEmpty() && !data->IsEmpty())
			{
			data->TransferTo(*characters);
			}

		}	// if (FileExists(fn.c_str()))

	else
		{
		outman.UserMessage("Sorry, could not find specified file: %s", fn.c_str());
		ret = 1;
		}
	return ret;
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called when the HELP command needs to be parsed from within the GarliReader block.
*/
void GarliReader::HandleHelp(
  NxsToken &token)	/* the token used to read from `in' */
	{
	// Retrieve all tokens for this command, stopping only in the event
	// of a semicolon or an unrecognized keyword
	//
	for (;;)
		{
		token.GetNextToken();

		if (token.Equals(";"))
			{
			break;
			}
		else
			{
			errormsg = "Unexpected keyword (";
			errormsg += token.GetToken();
			errormsg += ") encountered reading HELP command";
			throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
			}
		}

	message = "\nExamples of use of available commands:";
	message += "\n  help                     -> shows this message";
	message += "\n  log file=mylog.txt start -> opens log file named mylog.txt";
	message += "\n  log stop                 -> closes current log file";
	message += "\n  exe mydata.nex           -> executes nexus file mydata.nex";
	message += "\n  show                     -> reports on blocks currently stored";
	message += "\n  quit                     -> terminates application";
	message += "\n";
	PrintMessage();
	}

void GarliReader::HandleGarliReader(
  NxsToken &token){	/* the token used to read from `in' */
	
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called when the HELP command needs to be parsed from within the GarliReader block.
*/
void GarliReader::HandleShow(
  NxsToken &token)	/* the token used to read from `in' */
	{
	// Retrieve all tokens for this command, stopping only in the event
	// of a semicolon or an unrecognized keyword
	//
	for (;;)
		{
		token.GetNextToken();

		if (token.Equals(";"))
			break;
		else
			{
			errormsg = "Unexpected keyword (";
			errormsg += token.GetToken();
			errormsg += ") encountered reading HELP command";
			throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
			}
		}

	message = "\nNexus blocks currently stored:";
	PrintMessage();

	if (!taxa->IsEmpty())
		{
		cerr << "\n  TAXA block found" << endl;
		taxa->Report(cerr);
		if (logf_open)
			taxa->Report(logf);
		}

	if (!trees->IsEmpty())
		{
		cerr << "\n  TREES block found" << endl;
		trees->Report(cerr);
		if (logf_open)
			trees->Report(logf);
		}

	if (!assumptions->IsEmpty())
		{
		cerr << "\n  ASSUMPTIONS block found" << endl;
		assumptions->Report(cerr);
		if (logf_open)
			assumptions->Report(logf);
		}

	if (!distances->IsEmpty())
		{
		cerr << "\n  DISTANCES block found" << endl;
		distances->Report(cerr);
		if (logf_open)
			distances->Report(logf);
		}

	if (!characters->IsEmpty())
		{
		cerr << "\n  CHARACTERS block found" << endl;
		characters->Report(cerr);
		if (logf_open)
			characters->Report(logf);
		if (!charBlocks.empty())
			{
			cerr << "\n  " << charBlocks.size() << " CHARACTERS block found" << endl;
			for(vector<NxsCharactersBlock*>::iterator it=charBlocks.begin();it != charBlocks.end();it++){
				(*it)->Report(cerr);
				if (logf_open)
					(*it)->Report(logf);
				}
			}
		}

	if (!data->IsEmpty())
		{
		cerr << "\n  DATA block found" << endl;
		data->Report(cerr);
		if (logf_open)
			data->Report(logf);
		}
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called when the LOG command needs to be parsed from within the GarliReader block.
*/
void GarliReader::HandleLog(
  NxsToken &token)	/* the token used to read from `in' */
	{
	bool starting = false;
	bool stopping = false;
	bool appending = false;
	bool replacing = false;
	bool name_provided = false;
	NxsString logfname;

	// Retrieve all tokens for this command, stopping only in the event
	// of a semicolon or an unrecognized keyword
	//
	for (;;)
		{
		token.GetNextToken();

		if (token.Equals(";"))
			{
			break;
			}
		else if (token.Abbreviation("STOp"))
			{
			stopping = true;
			}
		else if (token.Abbreviation("STArt"))
			{
			starting = true;
			}
		else if (token.Abbreviation("Replace"))
			{
			replacing = true;
			}
		else if (token.Abbreviation("Append"))
			{
			appending = true;
			}
		else if (token.Abbreviation("File"))
			{
			logfname = GetFileName(token);
			name_provided = true;
			}
		else
			{
			errormsg = "Unexpected keyword (";
			errormsg += token.GetToken();
			errormsg += ") encountered reading LOG command";
			throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
			}
		}

	// Check for incompatible combinations of keywords
	//
	if (stopping && (starting || appending || replacing || name_provided))
		{
		errormsg = "Cannot specify STOP with any of the following START, APPEND, REPLACE, FILE";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	if (appending && replacing)
		{
		errormsg = "Cannot specify APPEND and REPLACE at the same time";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	if (logf_open && (starting || name_provided || appending || replacing))
		{
		errormsg = "Cannot start log file since log file is already open";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	// Is user closing an open log file?
	//
	if (stopping)
		{
		logf.close();
		logf_open = false;

		message = "\nLog file closed";
		PrintMessage();

		return;
		}

	// If this far, must be attempting to open a log file
	//
	if (!name_provided)
		{
		errormsg = "Must provide a file name when opening a log file\n";
		errormsg += "e.g., log file=doofus.txt start replace;";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}   

	if (appending)
		{
		logf_open = true;
		logf.open(logfname.c_str(), ios::out | ios::app);

		message = "\nAppending to log file ";
		message += logfname;
		PrintMessage();
		}

	else if (replacing)
		{
		logf_open = true;
		logf.open(logfname.c_str());

		message = "\nReplacing log file ";
		message += logfname;
		PrintMessage();
		}

	else 
		{
		bool exists = FileExists(logfname.c_str());
		bool userok = true;
		if (exists && !UserQuery("Ok to replace?", "Log file specified already exists", GarliReader::UserQueryEnum(GarliReader::uq_yes | GarliReader::uq_no)))
			userok = false;

		if (userok)
			{
			logf_open = true;
			logf.open(logfname.c_str());
			}

		if (exists && userok)
			{
			message = "\nReplacing log file ";
			message += logfname;
			}

		else if (userok)
			{
			message = "\nLog file ";
			message += logfname;
			message += " opened";
			}

		else
			{
			message = "\nLog command aborted";
			}

		PrintMessage();
		}
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Accepts a string in the form of a GarliReader block containing one command and processes it just like a real 
|	GarliReader block in a NEXUS data file.
*/
void GarliReader::HandleNextCommand()
	{
	std::istringstream cmdin(next_command);

	MyNexusToken token(cmdin);
	try
		{
		Read(token);
		}
	catch(NxsException x) 
		{
		NexusError(errormsg, x.pos, x.line, x.col);
		}
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called when an error is encountered in a NEXUS file. Allows program to give user details of the error as well as 
|	the precise location of the error.
*/
void GarliReader::NexusError(
  NxsString msg,	/* the error message */
  file_pos ,		/* the point in the NEXUS file where the error occurred */
  long line,		/* the line in the NEXUS file where the error occurred */
  long col)			/* the column in the NEXUS file where the error occurred */
	{
	message = "\n";
	message += msg;
	PrintMessage();

	if (1)
		{
		message = "Line:   ";
		message += line;
		PrintMessage();

		message = "Column: ";
		message += col;
		PrintMessage();
		}
	throw ErrorException("NCL encountered a problem reading the dataset.");
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Begins with the command just entered by the user, which is stored in the data member `next_command', adds a 
|	semicolon (if the user failed to supply one), and then adds the string "end;" so the whole bundle looks like a 
|	very short GarliReader block. This is then passed to HandleNextCommand, which processes it just like a real 
|	GarliReader block in a NEXUS data file.
*/
void GarliReader::PreprocessNextCommand()
	{
	// If user failed to add the terminating semicolon, we'll do it now. We will also remove the line feed
	// at the end and add the command "end;" to the end of the line (see explanation below).
	//
	unsigned len = strlen(next_command);
	assert(len > 0);

	// Remove any whitespace characters from end of string entered by user
	//
	unsigned i = len;
	while (i > 0 && next_command[i-1] == ' ' || next_command[i-1] == '\t' || next_command[i-1] == '\n')
		i--;

	// If character at position i - 1 is a semicolon, put '\0' terminator at position i;
	// otherwise, put a semicolon at position i and terminator at i + 1
	//
	if (next_command[i-1] != ';')
		{
		next_command[i] = ';';
		i++;
		}
	assert(i <= COMMAND_MAXLEN);
	next_command[i] = '\0';

	// Now add a semicolon at the beginning and terminate with an "END;" command 
	// so that we can pretend this is simply a very short private NEXUS block
	// containing only one command.  This allows us to simply use the Read 
	// function we inherited from the base class BstBase to process the command.
	//
	len = strlen(next_command);
	assert(len < COMMAND_MAXLEN-2);
	NxsString tmp = ";";
	tmp += next_command;
	tmp += "end;";
	strcpy(next_command, tmp.c_str());
	}

/*----------------------------------------------------------------------------------------------------------------------
|	All output is funneled through here. Writes string currently stored in `message' (a NxsString data member) to the 
|	output file stream, if open, and also to the console via cerr. Places a newline after the string if `linefeed' is 
|	true.
|	DJZ - funneling all messages through my OutputManager, which already outputs to the screen and a log file
*/
void GarliReader::PrintMessage(
  bool linefeed)	/* if true, places newline character after message */
	{
	if(linefeed)
		outman.UserMessage(message);
	else 
		outman.UserMessageNoCR(message);
/*	cerr << message;
	if (linefeed)
		cerr << endl;

	if (logf_open)
		{
		logf << message;
		if (linefeed)
			logf << endl;
		}
*/	
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Detaches all blocks, deletes them, creates new blocks, and finally adds the new blocks. Call this function if
|	you want to be sure that there is no data currently stored in any blocks.
*/
void GarliReader::PurgeBlocks()
	{
	if (blockList != NULL)
		{
		Detach(taxa);
		Detach(trees);
		Detach(assumptions);
		Detach(distances);
		Detach(characters);
		Detach(data);
		//"this" (the garli reader itself) is added to the block list to provide support for Garli blocks.
		//we can't delete it, but need to detach it
		Detach(this);
		}

	delete taxa;
	delete trees;
	delete assumptions;
	delete distances;
	delete characters;
	if (charBlocks.size() > 0)
		{
		for(vector<NxsCharactersBlock*>::iterator it = charBlocks.begin();it != charBlocks.end(); it++){
//			Detach(*it);
			delete *it;
//			charBlocks.erase(it);
			}
		charBlocks.clear();
		}
	delete data;

	taxa		= new NxsTaxaBlock();
	trees		= new NxsTreesBlock(taxa);
#if defined(NCL_MAJOR_VERSION) && (NCL_MAJOR_VERSION >= 2) && (NCL_MINOR_VERSION >= 1)
	trees->SetAllowImplicitNames(true);
#endif
	assumptions	= new NxsAssumptionsBlock(taxa);
	distances	= new NxsDistancesBlock(taxa);
	characters	= new NxsCharactersBlock(taxa, assumptions);
	data		= new NxsDataBlock(taxa, assumptions);

	Add(taxa);
	Add(trees);
	Add(assumptions);
	Add(distances);
	Add(characters);
	Add(data);
	Add(this);
	}

/*----------------------------------------------------------------------------------------------------------------------
|	This function provides the ability to read everything following the block name (which is read by the NxsReader 
|	object) to the END or ENDBLOCK statement. Characters are read from the input stream `in'. Overrides the virtual 
|	function in the base class.
*/
void GarliReader::Read(
  NxsToken &token)	/* the token used to read from `in' */
	{
	isEmpty = false;
	//if we already read a garli block with a model string, clear it
	modelString.clear();

	// This should be the semicolon after the block name
	//
	token.GetNextToken();

	if (!token.Equals(";"))
		{
		errormsg = "Expecting ';' after ";
		errormsg += id;
		errormsg += " block name, but found ";
		errormsg += token.GetToken();
		errormsg += " instead";
		throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
		}

	for (;;)
		{//only allowing three things to happen here 
		//1. endblock is reached, sucessfully exiting the garli block
		//2. something besides an endblock is read.  This is interpreted as part of the model string, with minimal error checking
		//3. eof is hit before an endblock
		token.GetNextToken();

		if (token.Abbreviation("ENdblock"))
			{
			HandleEndblock(token);
			break;
			}
		else if(token.AtEOF() == false){
			NxsString s = token.GetToken();
			if(s.size() > 1 && (s.IsADouble() == false && s.IsALong() == false)){
				errormsg = "Unexpected character(s) in Garli block.\n     See manual for model parameter format.";
				throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
				}
			if(token.IsPunctuationToken() == false){//toss semicolons and such
				modelString += token.GetToken();
				modelString += ' ';
				}
			}
		else
			{
			errormsg = "Unexpected end of file encountered before \"end;\" or\n     \"endblock;\" command in Garli block";
			throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
			}
		}
/*
		else if (token.Abbreviation("GarliReader"))
			{
			HandleGarliReader(token);
			}
		else if (token.Abbreviation("Help"))
			{
			HandleHelp(token);
			}
		else if (token.Abbreviation("Log"))
			{
			HandleLog(token);
			}
		else if (token.Abbreviation("EXecute"))
			{
			HandleExecute(token);
			}
		else if (token.Abbreviation("Show"))
			{
			HandleShow(token);
			}
		else if (token.Abbreviation("Quit"))
			{
			quit_now = true;

			message = "\nGarliReader says goodbye\n";
			PrintMessage();

			break;
			}
		else
			{
			SkippingCommand(token.GetToken());
			do
				{
				token.GetNextToken();
				}
			while (!token.AtEOF() && !token.Equals(";"));

			if (token.AtEOF())
				{
				errormsg = "Unexpected end of file encountered";
				throw NxsException(errormsg, token.GetFilePosition(), token.GetFileLine(), token.GetFileColumn());
				}
			}
*/
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Overrides the NxsBlock virtual function. This function does nothing because the GarliReader block is simply a
|	private command block and does not store any data.
*/

void GarliReader::Reset()
	{
	}

/*----------------------------------------------------------------------------------------------------------------------
|	This function outputs a brief report of the contents of this GarliReader block. Overrides the virtual function 
|	in the NxsBlock base class.
*/
void GarliReader::Report(
  ostream &out)	/* the output stream to which to write the report */
	{
	message.clear();
	PrintMessage();
	out << message << '\n';
	message = id;
	message += " block contains...";
	PrintMessage();
	out << message << '\n';
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Runs the command line interpreter, allowing GarliReader to interact with user. Typically, this is the only 
|	function called in main after a GarliReader object is created. If `infile_name' is non-NULL, the first command 
|	executed by the command interpreter will be "EXECUTE `infile_name'".
|  DJZ - not currently used, since I'm just using NCL to parse the datafile
*/
void GarliReader::Run(
  char *infile_name)	/* the name of the NEXUS data file to execute (can be NULL) */
	{
	taxa			= new NxsTaxaBlock();
	trees			= new NxsTreesBlock(taxa);
#if defined(NCL_MAJOR_VERSION) && (NCL_MAJOR_VERSION >= 2) && (NCL_MINOR_VERSION >= 1)
	trees->SetAllowImplicitNames(true);
#endif
	assumptions		= new NxsAssumptionsBlock(taxa);
	characters		= new NxsCharactersBlock(taxa, assumptions);
	distances		= new NxsDistancesBlock(taxa);
	data			= new NxsDataBlock(taxa, assumptions);

	Add(taxa);
	Add(trees);
	Add(assumptions);
	Add(characters);
	Add(distances);
	Add(data);
	Add(this);

	if (infile_name != NULL)
		{
		strcpy(next_command, "exe ");
		strncat(next_command, infile_name, 252);
		PreprocessNextCommand();
		HandleNextCommand();
		}

	quit_now = false;
	while (!quit_now) 
		{
		cerr << endl;
		cerr << "GarliReader> ";
		//cin.getline(next_command, COMMAND_MAXLEN);
		unsigned i = 0;
		for(;;)
			{
			int ch = cin.get();
			if (i > COMMAND_MAXLEN)
				{
				cerr << endl;
				cerr << "Error: the length of any one command cannot exceed ";
				cerr << COMMAND_MAXLEN << " characters" << endl;
				break;
				}
			else if (ch == 10 || ch == 13)
				break;
			next_command[i++] = (char)ch;
			next_command[i] = '\0';
			}
		PreprocessNextCommand();
		HandleNextCommand();
		}
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called when program does not recognize a block name encountered in a NEXUS file. Virtual function that overrides 
|	the virtual function in the base class NxsReader.
*/
void GarliReader::SkippingBlock(
  NxsString blockName)	/* the unrecognized block name */
	{
	message = "Skipping unknown block (";
	message += blockName;
	message += ")";
	PrintMessage();
	}

/*----------------------------------------------------------------------------------------------------------------------
|	This function is called when an unknown command named `commandName' is about to be skipped. This version of the 
|	function (which is identical to the base class version) does nothing (i.e., no warning is issued that a command 
|	was unrecognized). Modify this virtual function to provide such warnings to the user (or eliminate it altogether 
|	since the base class version already does what this does).
*/
void GarliReader::SkippingCommand(
  NxsString commandName)	/* the name of the command being skipped */
	{
	message = "Skipping unknown command (";
	message += commandName;
	message += ")";
	PrintMessage();
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called by the NxsReader object when skipping a block named blockName that has been disabled. Allows program to 
|	notify user of progress in parsing the NEXUS file. Virtual function that overrides the virtual function in the 
|	base class NxsReader.
*/
void GarliReader::SkippingDisabledBlock(
  NxsString )	/* the name of the block just exited */
	{
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Returns true if response is either "ok" or "yes", and returns false if response is either "no" or "cancel".
|	This is a general query function that can handle many situations. The possible responses are enumerated in 
|	GarliReader::UserQueryEnum: uq_cancel, uq_ok, uq_yes, and uq_no. Not yet fully implemented: only handles uq_ok
|	alone or the (uq_yes | uq_no) combination.
*/
bool GarliReader::UserQuery(
  NxsString mb_message,						/* the question posed to the user */
  NxsString mb_title,						/* the title of the message box */
  GarliReader::UserQueryEnum mb_choices)	/* bit combination of uq_xx values indicating which buttons to show */
	{
	const bool yes_no			= (mb_choices == (GarliReader::uq_yes | GarliReader::uq_no));
	const bool ok_only		= (mb_choices == GarliReader::uq_ok);
	assert(ok_only || yes_no); // Still working on other choices

	if (ok_only)
		{
		cerr << endl;
		cerr << mb_title << endl;
		cerr << "  " << mb_message;
		cerr << " (press return to acknowledge) ";
		cin.getline(next_command, COMMAND_MAXLEN);
		return true;
		}
	cerr << endl;
	cerr << mb_title << endl;
	cerr << "  " << mb_message;
	cerr << " (y/n) ";

	cin.getline(next_command, COMMAND_MAXLEN);

	// This could be made much simpler by just checking first letter: if 'y' then
	// assume yes, if 'n' assume no.
	//
	bool yep  = (next_command[0] == 'y' && next_command[1] == '\0');
	bool nope = (next_command[0] == 'n' && next_command[1] == '\0');

	while (!yep && !nope)
		{
		cerr << endl;
		cerr << "Must answer by typing either y or n and then pressing the Enter key" << endl;
		cerr << endl;
		cerr << mb_title << endl;
		cerr << "  " << mb_message;
		cerr << " (y/n) ";

		cin.getline(next_command, COMMAND_MAXLEN);
		yep  = (next_command[0] == 'y' && next_command[1] == '\0');
		nope = (next_command[0] == 'n' && next_command[1] == '\0');
		}

	return yep;
	}

/*----------------------------------------------------------------------------------------------------------------------
|	Called if an "output comment" is encountered in a NEXUS data file. An output comment is a comment [text enclosed in
|	square brackets] that begins with an exclamation point. [!This is an example of a NEXUS output comment]. Output
|	comments are supposed to be displayed when encountered. Modify this function's body to display output comments, 
|	which are made available as they are encountered via the `msg' argument.
*/
inline void	GarliReader::OutputComment(const NxsString &msg)
	{
	unsigned pos;
	string s;
	//changing this again - just eating the Garli output comments
	s = "GarliScore";
	pos = msg.find(s);
	if(pos != string::npos){
		//outman.UserMessage("This is apparently a tree inferred by Garli in a previous run.  Its score was %s", msg.substr(s.length()).c_str());
		return;
		}
	s = "GarliModel";
	pos = msg.find(s);
	if(pos != string::npos){
		//outman.UserMessage("Garli's model parameter values used in inferring this tree:\n\t%s", msg.substr(s.length()).c_str());
		return;
		}
	s =	"****NOTE";//this is a note about the parameter values either being from a run that was terimated early or that 
					//they are only optimal for a certain tree. This is mainly for output when reading the trees in PAUP
					//and we will just ignore them here
	pos = msg.find(s);
	if(pos != string::npos) return;
	
	outman.UserMessage("\nCOMMENT FOUND IN NEXUS FILE (output verbatim):");
	outman.UserMessage(msg);
	outman.UserMessage("(END OF NEXUS COMMENT)");
	}

GarliReader & GarliReader::GetInstance()
	{
	static GarliReader gr;
	return gr;
	}
