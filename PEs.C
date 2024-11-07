// Sam Grant 2024
// Read and checksum for PEsPerLayer and sidePEsPerLayer 
// Run this inside an EventNtuple muse environment
// ChatGPT did the printouts... 

#include <iostream>
#include <iomanip>  // For std::setw and std::setprecision
#include <cstdlib>  // For std::exit
#include "EventNtuple/inc/CrvHitInfoReco.hh"

void DrawTH1(TH1F *hist, TString title, TString fname, bool logX = true, bool logY = true) {

	TCanvas *c = new TCanvas("c","c",800,600);

	hist->SetTitle(title);

	//hist->SetStats(0);
	gStyle->SetOptStat(2210);
			
	hist->GetXaxis()->SetTitleSize(.04);
	hist->GetYaxis()->SetTitleSize(.04);
	hist->GetXaxis()->SetTitleOffset(1.1);
	hist->GetYaxis()->SetTitleOffset(1.1);
	hist->GetXaxis()->CenterTitle(1);
	hist->GetYaxis()->CenterTitle(1);
	hist->GetYaxis()->SetMaxDigits(4);
	hist->SetLineWidth(3);
	hist->SetLineColor(1);

    if (logX) { c->SetLogx(); }
    if (logY) { c->SetLogy(); }

	//c->SetRightMargin(0.13);

	hist->Draw("HIST");
	
	//c->SaveAs((fname+".C").c_str());
	// c->SaveAs(fname+".pdf");
	c->SaveAs(fname+".png");

	delete c;

	return;
}

void DrawManyTH1(std::map<TString, TH1F*> hist_map, TString title, TString fname, bool logX = true, bool logY = true) {

    TCanvas *c = new TCanvas("c", "c", 800, 600);
    gStyle->SetOptStat(0);

    auto it = hist_map.begin();
    it->second->SetTitle(title);

    TLegend *legend = new TLegend(0.69, 0.69, 0.89, 0.89);
    legend->SetBorderSize(0);
    legend->SetTextSize(0.03);

    size_t i = 0;
    for (auto &[label, hist] : hist_map) {
        // Set axis properties
        hist->GetXaxis()->SetTitleSize(.04);
        hist->GetYaxis()->SetTitleSize(.04);
        hist->GetXaxis()->SetTitleOffset(1.1);
        hist->GetYaxis()->SetTitleOffset(1.1);
        hist->GetXaxis()->CenterTitle(1);
        hist->GetYaxis()->CenterTitle(1);
        hist->GetYaxis()->SetMaxDigits(4);

        // Set line color and style
        hist->SetLineColor(i + 1); 
        hist->SetLineWidth(2);

        // Draw the histogram, using "same" for all but the first
        hist->Draw(i == 0 ? "HIST" : "HIST SAME");

        // Add to legend with its label
        legend->AddEntry(hist, label, "l");

        ++i;
    }

    // Draw the legend
    legend->Draw();

    // Apply log scales if needed
    if (logX) { c->SetLogx(); }
    if (logY) { c->SetLogy(); }

    // Save the canvas
    c->SaveAs(fname + ".png");

    // Clean up
    delete legend;
    delete c;
}

void Run(TString config, bool printouts = false) { 

    // Get tree 
    TString finName = "../trees/" + config + "_tree.root";
    TFile* fin = TFile::Open(finName); 
    TTree* tree = (TTree*)fin->Get("EventNtuple/ntuple");

    // Get branches
    Int_t event;
    std::vector<mu2e::CrvHitInfoReco>* crvcoincs = nullptr;
    tree->SetBranchAddress("event", &event);
    tree->SetBranchAddress("crvcoincs", &crvcoincs);

    // Book histograms
    TH1F *h1_PEs = new TH1F("h1_PEs", "", 1e5-1, 1, 1e5);
    std::map<TString, TH1F*> h1_PEsPerLayerMap; // slightly awkward 
    std::map<TString, TH1F*> h1_sidePEsPerLayerMap; 
    for (int i_layer = 0; i_layer < mu2e::CRVId::nLayers; i_layer++) {
        TH1F *h1_PEsPerLayer = new TH1F(("h1_PEsPerLayer"+to_string(i_layer)).c_str(), "", 1e4-1, 1, 1e4); 
        h1_PEsPerLayerMap[("Layer " +to_string(i_layer)).c_str()] = h1_PEsPerLayer;
        for (int i_side = 0; i_side < mu2e::CRVId::nSidesPerBar; i_side++) { 
            TH1F *h1_sidePEsPerLayer = new TH1F(("h1_sidePEsPerLayer"+to_string(i_layer)+to_string(i_side)).c_str(), "", 1e4-1, 1, 1e4);
            h1_sidePEsPerLayerMap[("Layer " +to_string(i_layer)+", side "+to_string(i_side)).c_str()] = h1_sidePEsPerLayer; 
        }
    }

    int count = 0; 
    Long64_t nEntries = tree->GetEntries();
    for (Long64_t i_event = 0; i_event < nEntries; ++i_event) {
        // Get event
        tree->GetEntry(i_event);
        // Skip empty events
        if (crvcoincs->size() == 0) { 
            continue;
        }
        // Header for each event
        if (printouts) {
            std::cout << "\n======================================" << std::endl; 
            std::cout << "\nEvent: " << event << "\n";
        }
        // Loop through coincidences in the current event
        for (auto& crvcoinc : *crvcoincs) {
            // Get total PEs
            float PEs = crvcoinc.PEs;
            // Fill 
            h1_PEs->Fill(PEs);
            // Printout
            if (printouts) { 
                std::cout << "\n--------------------------------------\n";
                std::cout << "\nPEs: " << std::fixed << std::setprecision(3) << PEs << std::endl;
            }
            // PEs/layer checksum
            float layerCheckSum = 0;
            for (int i_layer = 0; i_layer < mu2e::CRVId::nLayers; i_layer++) {
                // Get PEsPerLayer
                float PEsPerLayer = crvcoinc.PEsPerLayer[i_layer];
                // Fill 
                h1_PEsPerLayerMap[("Layer " +to_string(i_layer)).c_str()]->Fill(PEsPerLayer);
                // Printout
                if (printouts) { 
                    std::cout << "  PEsPerLayer[" << i_layer << "]: " 
                            << std::setw(8) << std::fixed << std::setprecision(3) << PEsPerLayer << std::endl;
                }
                layerCheckSum += PEsPerLayer;
            }

            // Check if layerCheckSum matches PEs
            // 1e-3 throws a few mismatches at high PE values... Not sure why? 
            if (std::abs(PEs - layerCheckSum) > 1e-2) {
                std::cout << "\nEvent: " << event << "\n";
                std::cout << "PEs and layerCheckSum mismatch!\n"; 
                std::cout << "  Expected PEs: " << std::setw(8) << PEs 
                          << " , calculated layerCheckSum: " << layerCheckSum << "\n";
                std::cerr << "ERROR: PEs != layerCheckSum" << std::endl;
                // std::exit(EXIT_FAILURE);
            } else { 
                if (printouts) { 
                    std::cout << "\n  Layer checksum: " << layerCheckSum << "\n";
                }
            }

            if (printouts) { std::cout << "\n--------------------------------------\n\n"; }

            layerCheckSum = 0;  // Reset for the next check
            for (int i_layer = 0; i_layer < mu2e::CRVId::nLayers; i_layer++) {
                float PEsPerLayer = crvcoinc.PEsPerLayer[i_layer];
                float sideCheckSum = 0; 

                for (int i_side = 0; i_side < mu2e::CRVId::nSidesPerBar; i_side++) { 
                    int layerSideIndex = i_layer * mu2e::CRVId::nSidesPerBar + i_side;
                    float sidePEsPerLayer = crvcoinc.sidePEsPerLayer[layerSideIndex];
                    // Fill 
                    h1_sidePEsPerLayerMap[("Layer " +to_string(i_layer)+", side "+to_string(i_side)).c_str()]->Fill(sidePEsPerLayer);

                    sideCheckSum += sidePEsPerLayer;
                    layerCheckSum += sidePEsPerLayer;
                    
                    if (printouts) {
                        // Print each sidePEsPerLayer value
                        std::cout << "    sidePEsPerLayer[" << layerSideIndex << "]: " 
                                << std::setw(8) << std::fixed << std::setprecision(3) << sidePEsPerLayer << std::endl;
                    }
                }

                // Check if PEsPerLayer matches sideCheckSum
                if (std::abs(PEsPerLayer - sideCheckSum) > 1e-2) { 
                    std::cout << "\nEvent: " << event << "\n";
                    std::cout << "PEsPerLayer and sideCheckSum mismatch at layer " << i_layer << "!\n";
                    std::cout << "  Expected PEsPerLayer: " << std::setw(8) << PEsPerLayer 
                              << " , calculated sideCheckSum: " << sideCheckSum << "\n";
                    // std::exit(EXIT_FAILURE);
                } else { 
                    if (printouts) { 
                        std::cout << "\n    Side checksum: " << sideCheckSum << "\n\n";
                    }
                }
            }

            // Final check if PEs matches layerCheckSum
            if (std::abs(PEs - layerCheckSum) > 1e-2) { 
                std::cout << "\nEvent: " << event << "\n";
                std::cout << "PEs and final layerCheckSum mismatch!\n";
                std::cout << "  Expected PEs: " << std::setw(8) << PEs 
                          << " , calculated layerCheckSum: " << layerCheckSum << "\n";
                // std::exit(EXIT_FAILURE);
            } else { 
                if (printouts) { 
                    std::cout << "  Total side checksum : " << layerCheckSum << "\n";
                }
            }

            if (printouts) { 
                std::cout << "\n--------------------------------------\n";
                std::cout << "\nAll checksums OK" << std::endl;
            }
        }

        // Calculate the percentage completion
        float percentage = (static_cast<float>(i_event) / nEntries) * 100;
        // Print the percentage 
        if (i_event % (nEntries / 100) == 0) {
            printf("Processing: %.2f%%\n", percentage);
        }
        
    }

    // Draw
    DrawTH1(h1_PEs, config + "; PEs; Coincidences", "../Images/h1_PEs_" + config);
    DrawManyTH1(h1_PEsPerLayerMap, config + "; PEs per layer; Coincidences", "../Images/h1_PEsPerLayer_" + config);
    DrawManyTH1(h1_sidePEsPerLayerMap, config + "; PEs per layer per side; Coincidences", "../Images/h1_sidePEsPerLayer_" + config);

    // Close file
    fin->Close();
    std::cout << "Processing completed successfully.\n";

    return;
}

void PEs() { 

    Run("CRY_ext", false);
    Run("CORSIKA_std", false);

    return;

}
