/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
  NNLS-Chroma / Chordino

  Audio feature extraction plugins for chromagram and chord
  estimation.

  Centre for Digital Music, Queen Mary University of London.
  This file copyright 2008-2010 Matthias Mauch and QMUL.
    
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.  See the file
  COPYING included with this distribution for more information.
*/

#include "Chordino.h"

#include "chromamethods.h"
#include "viterbi.h"
#include "seventhtransitions.h"

#include <cstdlib>
#include <fstream>
#include <cmath>

#include <algorithm>
#include <sstream>

const bool debug_on = false;

Chordino::Chordino(float inputSampleRate) :
    NNLSBase(inputSampleRate),
    m_chorddict(0),
    m_chordnotes(0),
    m_chordnames(0),
    m_extendedTemplates(true),
    m_diagnostics(false),
    m_seventhTransition(0.f)
{
    if (debug_on) cerr << "--> Chordino" << endl;
}

Chordino::~Chordino()
{
    if (debug_on) cerr << "--> ~Chordino" << endl;
}

string
Chordino::getIdentifier() const
{
    if (debug_on) cerr << "--> getIdentifier" << endl;
    return "chordino";
}

string
Chordino::getName() const
{
    if (debug_on) cerr << "--> getName" << endl;
    return "Chordino";
}

string
Chordino::getDescription() const
{
    if (debug_on) cerr << "--> getDescription" << endl;
    return "Dechorder's Chordino fork provides NNLS Chroma chord transcription with explicit no-evidence silence handling, unbiased first-frame decoding, and coalesced chord labels. Chord profiles are compared frame by frame and smoothed using an HMM/Viterbi decoder.";
}

int
Chordino::getPluginVersion() const
{
    return 10;
}

Chordino::ParameterList
Chordino::getParameterDescriptors() const
{
    if (debug_on) cerr << "--> getParameterDescriptors" << endl;
    ParameterList list;

    ParameterDescriptor useNNLSParam;
    useNNLSParam.identifier = "useNNLS";
    useNNLSParam.name = "use approximate transcription (NNLS)";
    useNNLSParam.description = "Toggles approximate transcription (NNLS).";
    useNNLSParam.unit = "";
    useNNLSParam.minValue = 0.0;
    useNNLSParam.maxValue = 1.0;
    useNNLSParam.defaultValue = 1.0;
    useNNLSParam.isQuantized = true;
	useNNLSParam.quantizeStep = 1.0;
    list.push_back(useNNLSParam);

    ParameterDescriptor rollonParam;
    rollonParam.identifier = "rollon";
    rollonParam.name = "bass noise threshold";
    rollonParam.description = "Consider the cumulative energy spectrum (from low to high frequencies). All bins below the first bin whose cumulative energy exceeds the quantile [bass noise threshold] x [total energy] will be set to 0. A threshold value of 0 means that no bins will be changed.";
    rollonParam.unit = "%";
    rollonParam.minValue = 0;
    rollonParam.maxValue = 5;
    rollonParam.defaultValue = 0.0;
    rollonParam.isQuantized = true;
	rollonParam.quantizeStep = 0.5;
    list.push_back(rollonParam);

    ParameterDescriptor tuningmodeParam;
    tuningmodeParam.identifier = "tuningmode";
    tuningmodeParam.name = "tuning mode";
    tuningmodeParam.description = "Tuning can be performed locally or on the whole extraction segment. Local tuning is only advisable when the tuning is likely to change over the audio, for example in podcasts, or in a cappella singing.";
    tuningmodeParam.unit = "";
    tuningmodeParam.minValue = 0;
    tuningmodeParam.maxValue = 1;
    tuningmodeParam.defaultValue = 0.0;
    tuningmodeParam.isQuantized = true;
    tuningmodeParam.valueNames.push_back("global tuning");
    tuningmodeParam.valueNames.push_back("local tuning");
    tuningmodeParam.quantizeStep = 1.0;
    list.push_back(tuningmodeParam);

    ParameterDescriptor whiteningParam;
    whiteningParam.identifier = "whitening";
    whiteningParam.name = "spectral whitening";
    whiteningParam.description = "Spectral whitening: no whitening - 0; whitening - 1.";
    whiteningParam.unit = "";
    whiteningParam.isQuantized = true;
    whiteningParam.minValue = 0.0;
    whiteningParam.maxValue = 1.0;
    whiteningParam.defaultValue = 1.0;
    whiteningParam.isQuantized = false;
    list.push_back(whiteningParam);

    ParameterDescriptor spectralShapeParam;
    spectralShapeParam.identifier = "s";
    spectralShapeParam.name = "spectral shape";
    spectralShapeParam.description = "Determines how individual notes in the note dictionary look: higher values mean more dominant higher harmonics.";
    spectralShapeParam.unit = "";
    spectralShapeParam.minValue = 0.5;
    spectralShapeParam.maxValue = 0.9;
    spectralShapeParam.defaultValue = 0.7;
    spectralShapeParam.isQuantized = false;
    list.push_back(spectralShapeParam);

    ParameterDescriptor boostnParam;
    boostnParam.identifier = "boostn";
    boostnParam.name = "boost N";
    boostnParam.description = "Boost likelihood of the N (no chord) label.";
    boostnParam.unit = "";
    boostnParam.minValue = 0.0;
    boostnParam.maxValue = 1.0;
    boostnParam.defaultValue = 0.1;
    boostnParam.isQuantized = false;
    list.push_back(boostnParam);

    ParameterDescriptor usehartesyntaxParam;
    usehartesyntaxParam.identifier = "usehartesyntax";
    usehartesyntaxParam.name = "use Harte syntax";
    usehartesyntaxParam.description = "Use the chord syntax proposed by Harte";
    usehartesyntaxParam.unit = "";
    usehartesyntaxParam.minValue = 0.0;
    usehartesyntaxParam.maxValue = 1.0;
    usehartesyntaxParam.defaultValue = 0.0;
    usehartesyntaxParam.isQuantized = true;
	usehartesyntaxParam.quantizeStep = 1.0;
	usehartesyntaxParam.valueNames.push_back("no");
    usehartesyntaxParam.valueNames.push_back("yes");
    list.push_back(usehartesyntaxParam);

    ParameterDescriptor extendedParam;
    extendedParam.identifier = "extendedchords";
    extendedParam.name = "include suspended, add9 and power chords";
    extendedParam.description = "Add sus2, sus4, add9 and power-chord templates to the built-in vocabulary. A custom chord.dict still defines its own complete vocabulary.";
    extendedParam.unit = "";
    extendedParam.minValue = 0.0;
    extendedParam.maxValue = 1.0;
    extendedParam.defaultValue = 1.0;
    extendedParam.isQuantized = true;
    extendedParam.quantizeStep = 1.0;
    extendedParam.valueNames.push_back("no");
    extendedParam.valueNames.push_back("yes");
    list.push_back(extendedParam);

    ParameterDescriptor diagnostics;
    diagnostics.identifier = "diagnostics";
    diagnostics.name = "emit observational decoder diagnostics";
    diagnostics.description = "Export exact note/chroma inputs, per-state raw scores and normalized emissions, native state choices and dictionary weights. Does not alter decoding.";
    diagnostics.unit = "";
    diagnostics.minValue = 0.f;
    diagnostics.maxValue = 1.f;
    diagnostics.defaultValue = 0.f;
    diagnostics.isQuantized = true;
    diagnostics.quantizeStep = 1.f;
    list.push_back(diagnostics);

    ParameterDescriptor seventh;
    seventh.identifier = "seventhtransition";
    seventh.name = "experimental seventh transition discount";
    seventh.description = "Fractional log-switch-cost discount only between same-root, same-labelled-bass major/7/maj7 or minor/m7 pairs. Experimental unnormalized transition potentials; zero preserves baseline.";
    seventh.unit = "";
    seventh.minValue = 0.f;
    seventh.maxValue = 1.f;
    seventh.defaultValue = 0.f;
    seventh.isQuantized = false;
    list.push_back(seventh);
    return list;
}

float
Chordino::getParameter(string identifier) const
{
    if (identifier == "extendedchords") return m_extendedTemplates ? 1.f : 0.f;
    if (identifier == "diagnostics") return m_diagnostics ? 1.f : 0.f;
    if (identifier == "seventhtransition") return m_seventhTransition;
    return NNLSBase::getParameter(identifier);
}

void
Chordino::setParameter(string identifier, float value)
{
    if (identifier == "seventhtransition") {
        m_seventhTransition = float(SeventhTransitions::validDiscount(value));
        return;
    }
    if (identifier == "diagnostics") {
        m_diagnostics = value >= 0.5f;
        return;
    }
    if (identifier == "extendedchords") {
        m_extendedTemplates = value >= 0.5f;
        return;
    }
    NNLSBase::setParameter(identifier, value);
}

Chordino::OutputList
Chordino::getOutputDescriptors() const
{
    if (debug_on) cerr << "--> getOutputDescriptors" << endl;
    OutputList list;
    
    int index = 0;

    float featureRate =
        (m_stepSize == 0) ? m_inputSampleRate/2048 : m_inputSampleRate/m_stepSize;
    
    OutputDescriptor d7;
    d7.identifier = "simplechord";
    d7.name = "Chord Estimate";
    d7.description = "Estimated chord times and labels.";
    d7.unit = "";
    d7.hasFixedBinCount = true;
    d7.binCount = 0;
    d7.hasKnownExtents = false;
    d7.isQuantized = false;
    d7.sampleType = OutputDescriptor::VariableSampleRate;
    d7.hasDuration = false;
    d7.sampleRate = featureRate;
    list.push_back(d7);
    m_outputChords = index++;
    
    OutputDescriptor chordnotes;
    chordnotes.identifier = "chordnotes";
    chordnotes.name = "Note Representation of Chord Estimate";
    chordnotes.description = "A simple representation of the estimated chord with bass note (if applicable) and chord notes.";
    chordnotes.unit = "MIDI units";
    chordnotes.hasFixedBinCount = true;
    chordnotes.binCount = 1;
    chordnotes.hasKnownExtents = true;
    chordnotes.minValue = 0;
    chordnotes.maxValue = 127;
    chordnotes.isQuantized = true;
    chordnotes.quantizeStep = 1;
    chordnotes.sampleType = OutputDescriptor::VariableSampleRate;
    chordnotes.hasDuration = true;
    chordnotes.sampleRate = featureRate;
    list.push_back(chordnotes);
    m_outputChordnotes = index++;
    
    OutputDescriptor d8;
    d8.identifier = "harmonicchange";
    d8.name = "Harmonic Change Value";
    d8.description = "An indication of the likelihood of harmonic change. Depends on the chord dictionary. Calculation is different depending on whether the Viterbi algorithm is used for chord estimation, or the simple chord estimate.";
    d8.unit = "";
    d8.hasFixedBinCount = true;
    d8.binCount = 1;
    d8.hasKnownExtents = false;
    d8.isQuantized = false;
    d8.sampleType = OutputDescriptor::FixedSampleRate;
    d8.sampleRate = featureRate;
    d8.hasDuration = false;
    list.push_back(d8);
    m_outputHarmonicChange = index++;
  
    OutputDescriptor loglikelihood;
    loglikelihood.identifier = "loglikelihood";
    loglikelihood.name = "Log-Likelihood of Chord Estimate";
    loglikelihood.description = "Logarithm of the likelihood value of the simple chord estimate.";
    loglikelihood.unit = "";
    loglikelihood.hasFixedBinCount = true;
    loglikelihood.binCount = 1;
    loglikelihood.hasKnownExtents = false;
    loglikelihood.isQuantized = false;
    loglikelihood.sampleType = OutputDescriptor::FixedSampleRate;
    loglikelihood.sampleRate = featureRate;
    loglikelihood.hasDuration = false;
    list.push_back(loglikelihood);
    m_outputLoglikelihood = index++;

    const char *roots[] = {"A", "Bb", "B", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab"};
    OutputDescriptor diagnostic;
    diagnostic.identifier = "noteactivations";
    diagnostic.name = "Octave-Specific Note Activations";
    diagnostic.description = "84 actual coefficients before chroma weighting/folding: MIDI21..104, A0..G#7. NNLS coefficients with useNNLS=1, direct semitone sums otherwise. Diagnostics only.";
    diagnostic.unit = "activation";
    diagnostic.hasFixedBinCount = true;
    diagnostic.binCount = 84;
    diagnostic.hasKnownExtents = false;
    diagnostic.isQuantized = false;
    diagnostic.sampleType = OutputDescriptor::FixedSampleRate;
    diagnostic.sampleRate = featureRate;
    diagnostic.hasDuration = false;
    for (int note = 0; note < 84; ++note) {
        ostringstream name;
        name << roots[note % 12] << ((note + 21) / 12 - 1);
        diagnostic.binNames.push_back(name.str());
    }
    list.push_back(diagnostic);
    m_outputNoteActivations = index++;

    diagnostic.identifier = "decoderchroma";
    diagnostic.name = "Decoder Scoring Chroma";
    diagnostic.description = "24 actual scorer inputs, bass A..Ab then treble A..Ab. Optional display-chroma normalization is not applied. Diagnostics only.";
    diagnostic.binCount = 24;
    diagnostic.binNames.clear();
    for (int band = 0; band < 2; ++band) {
        for (int root = 0; root < 12; ++root) {
            diagnostic.binNames.push_back(string(band == 0 ? "bass " : "treble ") + roots[root]);
        }
    }
    const vector<string> chromaBins = diagnostic.binNames;
    list.push_back(diagnostic);
    m_outputDecoderChroma = index++;

    diagnostic.identifier = "staterawscores";
    diagnostic.name = "Per-State Raw Scores";
    diagnostic.description = "One dot product per state, before N's 0.7 factor, clipping and exponentiation. Zero-based bin index matches statedictionary. Double scores are exported as Vamp float values. Diagnostics only.";
    diagnostic.unit = "template similarity";
    // Custom dictionaries and parameter-dependent vocabulary are known after
    // initialise. Before then report an unspecified bin count.
    diagnostic.hasFixedBinCount = !m_chordnames.empty();
    diagnostic.binCount = m_chordnames.size();
    diagnostic.binNames = m_chordnames;
    list.push_back(diagnostic);
    m_outputStateRawScores = index++;

    diagnostic.identifier = "stateemissions";
    diagnostic.name = "Per-State Normalized Emissions";
    diagnostic.description = "Actual HMM emissions after N scaling, clipping, exponentiation, zero-evidence silence gate and normalization. Double emissions exported as Vamp float values; indices match statedictionary. Diagnostics only.";
    diagnostic.unit = "emission";
    list.push_back(diagnostic);
    m_outputStateEmissions = index++;

    diagnostic.identifier = "statepath";
    diagnostic.name = "Native Decoder State Choices";
    diagnostic.description = "Native Viterbi state and local maximum-emission state for every frame. Local ties select lowest index. Indices match statedictionary; choices use native double precision before float export. Diagnostics only.";
    diagnostic.unit = "state index";
    diagnostic.hasFixedBinCount = true;
    diagnostic.binCount = 2;
    diagnostic.binNames = {"viterbi_state", "local_emission_winner"};
    diagnostic.isQuantized = true;
    diagnostic.quantizeStep = 1.f;
    list.push_back(diagnostic);
    m_outputStatePath = index++;

    diagnostic.identifier = "statedictionary";
    diagnostic.name = "Decoder State Dictionary";
    diagnostic.description = "One row per state at timestamp zero: state index,24 expanded normalized weights (bass A..Ab,treble A..Ab), and exact state label in the feature label. N weights include boostn; its later 0.7 score multiplier is separate. Diagnostics only.";
    diagnostic.unit = "";
    diagnostic.binCount = 25;
    diagnostic.binNames = {"state_index"};
    diagnostic.binNames.insert(diagnostic.binNames.end(), chromaBins.begin(), chromaBins.end());
    diagnostic.isQuantized = false;
    diagnostic.sampleType = OutputDescriptor::VariableSampleRate;
    list.push_back(diagnostic);
    m_outputStateDictionary = index++;
  
    return list;
}

bool
Chordino::initialise(size_t channels, size_t stepSize, size_t blockSize)
{	
    if (debug_on) {
        cerr << "--> initialise";
    }
    
    if (!NNLSBase::initialise(channels, stepSize, blockSize)) {
        return false;
    }
    m_chordnames = chordDictionary(&m_chorddict, &m_chordnotes, m_boostN,
                                  m_harte_syntax, m_extendedTemplates);
    return true;
}

void
Chordino::reset()
{
    if (debug_on) cerr << "--> reset";
    NNLSBase::reset();
}

Chordino::FeatureSet
Chordino::process(const float *const *inputBuffers, Vamp::RealTime timestamp)
{   
    if (debug_on) cerr << "--> process" << endl;

    NNLSBase::baseProcess(inputBuffers, timestamp);

    return FeatureSet();
}

Chordino::FeatureSet
Chordino::getRemainingFeatures()
{
    // cerr << hw[0] << hw[1] << endl;
    if (debug_on) cerr << "--> getRemainingFeatures" << endl;    
    FeatureSet fsOut;
    if (m_logSpectrum.size() == 0) return fsOut;
    int nChord = m_chordnames.size();
    if (m_diagnostics) {
        for (int state = 0; state < nChord; ++state) {
            Feature entry;
            entry.hasTimestamp = true;
            entry.timestamp = Vamp::RealTime::zeroTime;
            entry.label = m_chordnames[state];
            entry.values.push_back(float(state));
            entry.values.insert(entry.values.end(), m_chorddict.begin() + 24 * state,
                                m_chorddict.begin() + 24 * (state + 1));
            fsOut[m_outputStateDictionary].push_back(entry);
        }
    }
    // 
    /**  Calculate Tuning
         calculate tuning from (using the angle of the complex number defined by the 
         cumulative mean real and imag values)
    **/
    float meanTuningImag = 0;
    float meanTuningReal = 0;
    for (int iBPS = 0; iBPS < nBPS; ++iBPS) {
        meanTuningReal += m_meanTunings[iBPS] * cosvalues[iBPS];
        meanTuningImag += m_meanTunings[iBPS] * sinvalues[iBPS];
    }
    float cumulativetuning = 440 * pow(2,atan2(meanTuningImag, meanTuningReal)/(24*M_PI));
    float normalisedtuning = atan2(meanTuningImag, meanTuningReal)/(2*M_PI);
    int intShift = floor(normalisedtuning * 3);
    float floatShift = normalisedtuning * 3 - intShift; // floatShift is a really bad name for this
		    
    char buffer0 [50];
		
    sprintf(buffer0, "estimated tuning: %0.1f Hz", cumulativetuning);
		    
		    
    /** Tune Log-Frequency Spectrogram
        calculate a tuned log-frequency spectrogram (currentTunedSpec): use the tuning estimated above (kinda f0) to 
        perform linear interpolation on the existing log-frequency spectrogram (kinda currentLogSpectrum).
    **/
    if (debug_on) cerr << endl << "[Chordino Plugin] Tuning Log-Frequency Spectrogram ... ";
					
    int count = 0;
		
    FeatureList tunedSpec;
    int nFrame = m_logSpectrum.size();
    
    vector<Vamp::RealTime> timestamps;

    for (FeatureList::iterator i = m_logSpectrum.begin(); i != m_logSpectrum.end(); ++i) {
        Feature currentLogSpectrum = *i;
        Feature currentTunedSpec; // tuned log-frequency spectrum
        currentTunedSpec.hasTimestamp = true;
        currentTunedSpec.timestamp = currentLogSpectrum.timestamp;
        timestamps.push_back(currentLogSpectrum.timestamp);
        currentTunedSpec.values.push_back(0.0); currentTunedSpec.values.push_back(0.0); // set lower edge to zero
		
        if (m_tuneLocal) {
            intShift = floor(m_localTuning[count] * 3);
            floatShift = m_localTuning[count] * 3 - intShift; // floatShift is a really bad name for this
        }
		        
        // cerr << intShift << " " << floatShift << endl;
		        
        for (int k = 2; k < (int)currentLogSpectrum.values.size() - 3; ++k) { // interpolate all inner bins
            float tempValue = currentLogSpectrum.values[k + intShift] * (1-floatShift) + currentLogSpectrum.values[k+intShift+1] * floatShift;
            currentTunedSpec.values.push_back(tempValue);
        }
		        
        currentTunedSpec.values.push_back(0.0); currentTunedSpec.values.push_back(0.0); currentTunedSpec.values.push_back(0.0); // upper edge
        vector<float> runningmean = SpecialConvolution(currentTunedSpec.values,hw);
        vector<float> runningstd;
        for (int i = 0; i < nNote; i++) { // first step: squared values into vector (variance)
            runningstd.push_back((currentTunedSpec.values[i] - runningmean[i]) * (currentTunedSpec.values[i] - runningmean[i]));
        }
        runningstd = SpecialConvolution(runningstd,hw); // second step convolve
        for (int i = 0; i < nNote; i++) { 
            runningstd[i] = sqrt(runningstd[i]); // square root to finally have running std
            if (runningstd[i] > 0) {
                // currentTunedSpec.values[i] = (currentTunedSpec.values[i] / runningmean[i]) > thresh ? 
                // 		                    (currentTunedSpec.values[i] - runningmean[i]) / pow(runningstd[i],m_whitening) : 0;
                currentTunedSpec.values[i] = (currentTunedSpec.values[i] - runningmean[i]) > 0 ?
                    (currentTunedSpec.values[i] - runningmean[i]) / pow(runningstd[i],m_whitening) : 0;
            }
            if (currentTunedSpec.values[i] < 0) {
                cerr << "ERROR: negative value in logfreq spectrum" << endl;
            }
        }
        tunedSpec.push_back(currentTunedSpec);
        count++;
    }
    if (debug_on) cerr << "done." << endl;
	    
    /** Semitone spectrum and chromagrams
        Semitone-spaced log-frequency spectrum derived from the tuned log-freq spectrum above. the spectrum
        is inferred using a non-negative least squares algorithm.
        Three different kinds of chromagram are calculated, "treble", "bass", and "both" (which means 
        bass and treble stacked onto each other).
    **/
    if (m_useNNLS == 0) {
        if (debug_on) cerr << "[Chordino Plugin] Mapping to semitone spectrum and chroma ... ";
    } else {
        if (debug_on) cerr << "[Chordino Plugin] Performing NNLS and mapping to chroma ... ";
    }

	    
    vector<vector<double> > chordogram;
    vector<vector<int> > scoreChordogram;
    vector<float> chordchange = vector<float>(tunedSpec.size(),0);
    count = 0;

    FeatureList chromaList;
    
    bool clipwarned = false;

    for (FeatureList::iterator it = tunedSpec.begin(); it != tunedSpec.end(); ++it) {
        Feature currentTunedSpec = *it; // logfreq spectrum
        Feature currentChromas; // treble and bass chromagram

        currentChromas.hasTimestamp = true;
        currentChromas.timestamp = currentTunedSpec.timestamp;    

        float b[nNote];
	
        bool some_b_greater_zero = false;
        float sumb = 0;
        for (int i = 0; i < nNote; i++) {
            // b[i] = m_dict[(nNote * count + i) % (nNote * 84)];
            b[i] = currentTunedSpec.values[i];
            sumb += b[i];
            if (b[i] > 0) {
                some_b_greater_zero = true;
            }            
        }
	    
        // here's where the non-negative least squares algorithm calculates the note activation x
	
        vector<float> chroma = vector<float>(12, 0);
        vector<float> basschroma = vector<float>(12, 0);
        vector<float> noteActivation(m_diagnostics ? 84 : 0, 0.f);
        float currval;
        int iSemitone = 0;
			
        if (some_b_greater_zero) {
            if (m_useNNLS == 0) {
                for (int iNote = nBPS/2 + 2; iNote < nNote - nBPS/2; iNote += nBPS) {
                    currval = 0;
                    for (int iBPS = -nBPS/2; iBPS < nBPS/2+1; ++iBPS) {
                        currval += b[iNote + iBPS] * (1-abs(iBPS*1.0/(nBPS/2+1)));						
                    }
                    chroma[iSemitone % 12] += currval * treblewindow[iSemitone];
                    basschroma[iSemitone % 12] += currval * basswindow[iSemitone];
                    if (m_diagnostics) noteActivation[iSemitone] = currval;
                    iSemitone++;
                }
		        
            } else {
                float x[84+1000];
                for (int i = 1; i < 1084; ++i) x[i] = 1.0;
                vector<int> signifIndex;
                int index=0;
                sumb /= 84.0;
                for (int iNote = nBPS/2 + 2; iNote < nNote - nBPS/2; iNote += nBPS) {
                    float currval = 0;
                    for (int iBPS = -nBPS/2; iBPS < nBPS/2+1; ++iBPS) {
                        currval += b[iNote + iBPS]; 
                    }
                    if (currval > 0) signifIndex.push_back(index);
                    index++;
                }
                float rnorm;
                float w[84+1000];
                float zz[84+1000];
                int indx[84+1000];
                int mode;
                int dictsize = nNote*signifIndex.size();
                // cerr << "dictsize is " << dictsize << "and values size" << f3.values.size()<< endl;
                float *curr_dict = new float[dictsize];
                for (int iNote = 0; iNote < (int)signifIndex.size(); ++iNote) {
                    for (int iBin = 0; iBin < nNote; iBin++) {
                        curr_dict[iNote * nNote + iBin] = 1.0 * m_dict[signifIndex[iNote] * nNote + iBin];
                    }
                }
                nnls(curr_dict, nNote, nNote, signifIndex.size(), b, x, &rnorm, w, zz, indx, &mode);
                delete [] curr_dict;
                for (int iNote = 0; iNote < (int)signifIndex.size(); ++iNote) {
                    // cerr << mode << endl;
                    chroma[signifIndex[iNote] % 12] += x[iNote] * treblewindow[signifIndex[iNote]];
                    basschroma[signifIndex[iNote] % 12] += x[iNote] * basswindow[signifIndex[iNote]];
                    if (m_diagnostics) noteActivation[signifIndex[iNote]] = x[iNote];
                }
            }	
        }

        vector<float> origchroma = chroma;
        chroma.insert(chroma.begin(), basschroma.begin(), basschroma.end()); // just stack the both chromas 
        currentChromas.values = chroma;

        if (m_doNormalizeChroma > 0) {
            vector<float> chromanorm = vector<float>(3,0);			
            switch (int(m_doNormalizeChroma)) {
            case 0: // should never end up here
                break;
            case 1:
                chromanorm[0] = *max_element(origchroma.begin(), origchroma.end());
                chromanorm[1] = *max_element(basschroma.begin(), basschroma.end());
                chromanorm[2] = max(chromanorm[0], chromanorm[1]);
                break;
            case 2:
                for (vector<float>::iterator it = chroma.begin(); it != chroma.end(); ++it) {
                    chromanorm[2] += *it; 						
                }
                break;
            case 3:
                for (vector<float>::iterator it = chroma.begin(); it != chroma.end(); ++it) {
                    chromanorm[2] += pow(*it,2); 						
                }
                chromanorm[2] = sqrt(chromanorm[2]);
                break;
            }
            if (chromanorm[2] > 0) {
                for (int i = 0; i < (int)chroma.size(); i++) {
                    currentChromas.values[i] /= chromanorm[2];
                }
            }
        }

        chromaList.push_back(currentChromas);

        // local chord estimation
        vector<double> currentChordSalience;
        double tempchordvalue = 0;
        double sumchordvalue = 0;
        Feature rawScores;
        if (m_diagnostics) {
            Feature notes;
            notes.hasTimestamp = true;
            notes.timestamp = currentChromas.timestamp;
            notes.values = noteActivation;
            fsOut[m_outputNoteActivations].push_back(notes);
            Feature inputs;
            inputs.hasTimestamp = true;
            inputs.timestamp = currentChromas.timestamp;
            inputs.values = chroma;
            fsOut[m_outputDecoderChroma].push_back(inputs);
            rawScores.hasTimestamp = true;
            rawScores.timestamp = currentChromas.timestamp;
        }
	        
        for (int iChord = 0; iChord < nChord; iChord++) {
            tempchordvalue = 0;
            for (int iBin = 0; iBin < 12; iBin++) {
                tempchordvalue += m_chorddict[24 * iChord + iBin] * chroma[iBin];
            }
            for (int iBin = 12; iBin < 24; iBin++) {
                tempchordvalue += m_chorddict[24 * iChord + iBin] * chroma[iBin];
            }
            if (m_diagnostics) rawScores.values.push_back(float(tempchordvalue));
            if (iChord == nChord-1) tempchordvalue *= .7;
            if (tempchordvalue < 0) tempchordvalue = 0.0;
            if (tempchordvalue > 200.0) {
                if (!clipwarned) {
                    cerr << "WARNING: interim chroma contains extreme chord value " << tempchordvalue << ", clipping this and any others that appear" << endl;
                    clipwarned = true;
                }
                tempchordvalue = 200.0;
            }
            tempchordvalue = pow(1.3, tempchordvalue);
            sumchordvalue += tempchordvalue;
            currentChordSalience.push_back(tempchordvalue);
        }
        if (!some_b_greater_zero) {
            // A zero spectrum is evidence for no chord, not an equally likely
            // observation of every chord. Uniform emissions let the HMM carry
            // chords across silence (or introduce a future chord too early).
            // Reuse the front end's existing evidence gate; add no new level
            // threshold here.
            currentChordSalience.assign(nChord, 0.0);
            currentChordSalience[nChord-1] = 1.0;
        } else if (sumchordvalue > 0) {
            for (int iChord = 0; iChord < nChord; iChord++) {
                currentChordSalience[iChord] /= sumchordvalue;
            }
        } else {
            currentChordSalience[nChord-1] = 1.0;
        }
        chordogram.push_back(currentChordSalience);
        if (m_diagnostics) {
            fsOut[m_outputStateRawScores].push_back(rawScores);
            Feature emissions;
            emissions.hasTimestamp = true;
            emissions.timestamp = currentChromas.timestamp;
            emissions.values.assign(currentChordSalience.begin(), currentChordSalience.end());
            fsOut[m_outputStateEmissions].push_back(emissions);
        }
	        
        count++;
    }
    if (debug_on) cerr << "done." << endl;
		
    vector<Feature> oldnotes;

    if (debug_on) cerr << "[Chordino Plugin] HMM Chord Estimation ... ";
    int oldchord = -1;
 
    // Let the audio choose the first state. A forced N prior cannot be
    // overturned by any amount of first-frame chord evidence.
    vector<double> init = vector<double>(nChord,1.0/nChord);
    
    double *delta;
    delta = (double *)malloc(sizeof(double)*nFrame*nChord);                
    
    vector<vector<double> > trans = SeventhTransitions::matrix(m_chordnames, m_seventhTransition);
    vector<double> scale;
    vector<int> chordpath = ViterbiPath(init, trans, chordogram, delta, &scale);
    
    chordchange[0] = 0;
    for (int iFrame = 0; iFrame < (int)chordpath.size(); ++iFrame) {
        if (m_diagnostics) {
            Feature path;
            path.hasTimestamp = true;
            path.timestamp = timestamps[iFrame];
            path.values.push_back(float(chordpath[iFrame]));
            const int localWinner = int(max_element(chordogram[iFrame].begin(),
                chordogram[iFrame].end()) - chordogram[iFrame].begin());
            path.values.push_back(float(localWinner));
            fsOut[m_outputStatePath].push_back(path);
        }
        if (chordpath[iFrame] != oldchord ) {
            // Different internal voicings can have the same displayed name.
            // Emit only real label changes, but retain note-voicing changes.
            if (oldchord < 0 ||
                m_chordnames[chordpath[iFrame]] != m_chordnames[oldchord]) {
                Feature chord_feature;
                chord_feature.hasTimestamp = true;
                chord_feature.timestamp = timestamps[iFrame];
                chord_feature.label = m_chordnames[chordpath[iFrame]];
                fsOut[m_outputChords].push_back(chord_feature);
            }
            oldchord = chordpath[iFrame];
            // chord notes
            for (int iNote = 0; iNote < (int)oldnotes.size(); ++iNote) { // finish duration of old chord
                oldnotes[iNote].duration = oldnotes[iNote].duration + timestamps[iFrame];
                fsOut[m_outputChordnotes].push_back(oldnotes[iNote]);
            }
            oldnotes.clear();
            for (int iNote = 0; iNote < (int)m_chordnotes[chordpath[iFrame]].size(); ++iNote) { // prepare notes of current chord
                Feature chordnote_feature;
                chordnote_feature.hasTimestamp = true;
                chordnote_feature.timestamp = timestamps[iFrame];
                chordnote_feature.values.push_back(m_chordnotes[chordpath[iFrame]][iNote]);
                chordnote_feature.hasDuration = true;
                chordnote_feature.duration = -timestamps[iFrame]; // this will be corrected at the next chord
                oldnotes.push_back(chordnote_feature);
            }
        }
        if (iFrame == 0) continue;
        /* calculating simple chord change prob */
        for (int iChord = 0; iChord < nChord; iChord++) {
            double num = delta[(iFrame-1) * nChord + iChord];
            double denom = delta[iFrame * nChord + iChord];
            double eps = 1e-7;
            if (denom < eps) denom = eps;
            chordchange[iFrame-1] += num * log(num / denom + eps);
        }
    }
    
    float logscale = 0;
    for (int iFrame = 0; iFrame < nFrame; ++iFrame) {
        logscale -= log(scale[iFrame]);
        Feature loglikelihood;
        loglikelihood.hasTimestamp = true;
        loglikelihood.timestamp = timestamps[iFrame];
        loglikelihood.values.push_back(-log(scale[iFrame]));
        // cerr << chordchange[iFrame] << endl;
        fsOut[m_outputLoglikelihood].push_back(loglikelihood);
    }
    logscale /= nFrame;

    // A terminal marker is not an observed N chord. Leave the final label
    // open for the host to extend to the known audio duration. The Vamp API
    // does not supply the precise input end here, so bound the note events
    // by one analysis hop after the last frame timestamp.
    Vamp::RealTime noteEnd = timestamps.back() +
        Vamp::RealTime::frame2RealTime(m_stepSize, m_inputSampleRate);
    for (int iNote = 0; iNote < (int)oldnotes.size(); ++iNote) { // finish duration of old chord
        oldnotes[iNote].duration = oldnotes[iNote].duration + noteEnd;
        fsOut[m_outputChordnotes].push_back(oldnotes[iNote]);
    }
    
    if (debug_on) cerr << "done." << endl;

    for (int iFrame = 0; iFrame < nFrame; iFrame++) {
        Feature chordchange_feature;
        chordchange_feature.hasTimestamp = true;
        chordchange_feature.timestamp = timestamps[iFrame];
        chordchange_feature.values.push_back(chordchange[iFrame]);
//        cerr << "putting value " << chordchange[iFrame] << " at time " << chordchange_feature.timestamp << endl;
        fsOut[m_outputHarmonicChange].push_back(chordchange_feature);
    }

    free(delta);
    
    // for (int iFrame = 0; iFrame < nFrame; iFrame++) cerr << fsOut[m_outputHarmonicChange][iFrame].values[0] << endl;
    
    return fsOut;     
}
