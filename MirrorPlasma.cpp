
#include "MirrorPlasma.hpp"
#include <cmath>
#include <iostream>
#include <boost/math/tools/roots.hpp>
#include <functional>
#include "TransitionFunction.hpp"
#include "BatchRunner.hpp"

MirrorPlasma::VacuumMirrorConfiguration::VacuumMirrorConfiguration(const std::map<std::string, double>& parameterMap, std::string FuelName, 
	bool rThrust, tribool AHeating, tribool rDiagnostics, bool ambiPolPhi, bool collisions, bool includeCXLosses, std::string asciiOut, std::string netCdfOut)
	: AmbipolarPhi(ambiPolPhi), IncludeCXLosses(includeCXLosses), Collisional(collisions), OutputFile(asciiOut), NetcdfOutputFile(netCdfOut)
{
	if( parameterMap.find("CentralCellField") != parameterMap.end())
		CentralCellFieldStrength = parameterMap.at("CentralCellField");

	if( parameterMap.find("MirrorRatio") != parameterMap.end())
		MirrorRatio = parameterMap.at("MirrorRatio");
	else if( parameterMap.find("ThroatField") != parameterMap.end())
		MirrorRatio = parameterMap.at("ThroatField")/CentralCellFieldStrength;

	if( parameterMap.find("PlasmaRadiusMin") != parameterMap.end())
		AxialGapDistance = parameterMap.at("PlasmaRadiusMin");
	else if( parameterMap.find("AxialGapDistance") != parameterMap.end())
		AxialGapDistance = parameterMap.at("AxialGapDistance");

	if( parameterMap.find("PlasmaColumnWidth") != parameterMap.end())
		PlasmaColumnWidth = parameterMap.at("PlasmaColumnWidth");
	else if(parameterMap.find("PlasmaRadiusMax") != parameterMap.end())
		PlasmaColumnWidth = parameterMap.at("PlasmaRadiusMax") - AxialGapDistance;

	if( parameterMap.find("Voltage") != parameterMap.end())
		ImposedVoltage = parameterMap.at("Voltage");

	if( parameterMap.find("WallRadius") != parameterMap.end())
		WallRadius = parameterMap.at("WallRadius");

	if( parameterMap.find("PlasmaLength") != parameterMap.end())
		PlasmaLength = parameterMap.at("PlasmaLength");

	if( parameterMap.find("AuxiliaryHeating") != parameterMap.end())
		AuxiliaryHeating = parameterMap.at("AuxiliaryHeating");

	if( parameterMap.find("ParallelFudgeFactor") != parameterMap.end())
		ParallelFudgeFactor = parameterMap.at("ParallelFudgeFactor");

	if( parameterMap.find("PerpFudgeFactor") != parameterMap.end())
		PerpFudgeFactor = parameterMap.at("PerpFudgeFactor");

	if( parameterMap.find("InitialTemp") != parameterMap.end())
		InitialTemp = parameterMap.at("InitialTemp");

	if( parameterMap.find("InitialMach") != parameterMap.end())
		InitialMach = parameterMap.at("InitialMach");

	if( parameterMap.find("SundialsAbsTol") != parameterMap.end())
		SundialsAbsTol = parameterMap.at("SundialsAbsTol");

	if( parameterMap.find("SundialsRelTol") != parameterMap.end())
		SundialsRelTol = parameterMap.at("SundialsRelTol");

	if( parameterMap.find("RateThreshold") != parameterMap.end())
		RateThreshold = parameterMap.at("RateThreshold");

	if ( FuelName == "Hydrogen" ) {
		IonSpecies.Mass   = 1.0;
		IonSpecies.Charge = 1.0;
		IonSpecies.Name   = "Hydrogen";
		AlphaHeating = false;
		ReportNuclearDiagnostics = false;
	} else if ( FuelName == "Deuterium" ) {
		IonSpecies.Mass   = 2.0;
		IonSpecies.Charge = 1.0;
		IonSpecies.Name   = "Deuterium";
		AlphaHeating = false;
		ReportNuclearDiagnostics = true;
	} else if ( FuelName == "DT Fuel" ) {
		IonSpecies.Mass   = 2.5;
		IonSpecies.Charge = 1.0;
		IonSpecies.Name   = "Deuterium/Tritium Fuel";
		AlphaHeating = true;
		ReportNuclearDiagnostics = true;
	} else {
		std::string ErrorMessage = "[error] Fuel is not a recognized plasma species";
		throw std::invalid_argument( ErrorMessage );
	}

	// If specified in the config file these values override the defaults from the fuel
	if(AHeating == tribool::tru) AlphaHeating = true;
	else if(AHeating == tribool::fal) AlphaHeating = false;

	if(rDiagnostics == tribool::tru) ReportNuclearDiagnostics = true;
	else if(rDiagnostics == tribool::fal) ReportNuclearDiagnostics = false;
}

/* Replaced by Batch Runner 
MirrorPlasma::MirrorPlasma( toml::value const& plasmaConfig )
	: pVacuumConfig( std::make_shared<VacuumMirrorConfiguration>( plasmaConfig ) )
{
	const auto mirrorConfig = toml::find<toml::value>( plasmaConfig, "configuration" );

	double TiTe = toml::find_or<double>( mirrorConfig, "IonToElectronTemperatureRatio", 0.0 );
	if ( TiTe < 0.0 )
	{
		throw std::invalid_argument( "[error] Ion to Electron temperature ratio must be a positive number" );
	}

	// Default to pure plasma
	Zeff = toml::find_or<double>( mirrorConfig, "Zeff", 1.0 );
	if ( Zeff <= 0.0 ) {
		throw std::invalid_argument( "[error] Effective charge (Z_eff) must be positive!" );
	}

	try {
		ElectronDensity = toml::find<double>( mirrorConfig, "ElectronDensity" );
	} catch ( std::out_of_range &e ) {
		throw std::invalid_argument( "[error] You must specify the electron density (ElectronDensity) in the [configuration] block" );
	}

	if ( ElectronDensity <= 0.0 ) {
		throw std::invalid_argument( "[error] Electron density must be positive!" );
	}

	IonDensity = ElectronDensity / pVacuumConfig->IonSpecies.Charge;

	// If the temperature is -1.0, that indicates we are in a Temperature Solve run
	// and the temperature will be solved for.
	if ( mirrorConfig.count( "ElectronTemperature" ) == 1 ) {
		ElectronTemperature = toml::find<double>( mirrorConfig, "ElectronTemperature" );
		IonTemperature = ElectronTemperature * TiTe;
	} else if ( mirrorConfig.count( "ElectronTemperature" ) == 0 ) {
		ElectronTemperature = -1.0;
		IonTemperature = -1.0;
	} else {
		throw std::invalid_argument( "[error] Electron Temperature specified more than once!" );
	}

	if ( mirrorConfig.count( "NeutralDensity" ) == 1 ) {
		NeutralSource = 0;
		NeutralDensity = mirrorConfig.at( "NeutralDensity" ).as_floating();
		FixedNeutralDensity = true;
	} else {
		NeutralSource = 0;
		NeutralDensity = 0;
		FixedNeutralDensity = false;
	}

	if ( mirrorConfig.count( "VoltageTrace" ) == 1 ) {
		ReadVoltageFile( mirrorConfig.at( "VoltageTrace" ).as_string() );
		isTimeDependent = true;
		SetTime( 0 );
	} else {
		isTimeDependent = false;
		time = -1;
	}
}
*/

MirrorPlasma::MirrorPlasma(std::shared_ptr< VacuumMirrorConfiguration > pVacuumConfig, std::map<std::string,double> parameterMap, std::string vTrace)
	: pVacuumConfig(pVacuumConfig)
{
	if( parameterMap.find("Zeff") != parameterMap.end())
		Zeff = parameterMap.at("Zeff");
	else Zeff = 1.0;

	if( parameterMap.find("ElectronDensity") != parameterMap.end())
		ElectronDensity = parameterMap.at("ElectronDensity");

	IonDensity = ElectronDensity / pVacuumConfig->IonSpecies.Charge; 

	double TiTe = 0.0;
	if( parameterMap.find("IonToElectronTemperatureRatio") != parameterMap.end())
		TiTe = parameterMap.at("IonToElectronTemperatureRatio");


	if( parameterMap.find("ElectronTemperature") != parameterMap.end())
	{
		ElectronTemperature = parameterMap.at("ElectronTemperature");
		IonTemperature = ElectronTemperature < 0.0 ? -1.0 : ElectronTemperature * TiTe;
	}
	// Note if(...) only false if theres a bug. Batch runner will populate the value as -1.0 if its not found in the config file
	else
	{
		ElectronTemperature = -1.0;
		IonTemperature = -1.0;
	}

	// Note: current excecution has NeutralSource always initially set to 0. If this changes work will have to be done in BatchRunner
	if( parameterMap.find("NeutralDensity") != parameterMap.end())
	{
		NeutralDensity = parameterMap.at("NeutralDensity");
		NeutralSource = 0.0;
		if ( NeutralDensity == 0.0 )
			FixedNeutralDensity = false;
		else
			FixedNeutralDensity = true;
	}
	else
	{
		NeutralDensity = 0.0;
		NeutralSource = 0.0;
		FixedNeutralDensity = false;
	}

	if(!vTrace.empty())
	{
		ReadVoltageFile( vTrace );
		isTimeDependent = true;
		SetTime( 0 );
	}
	else
	{
		isTimeDependent = false;
		time = -1;
	}
}

// From NRL Formulary p34
double MirrorPlasma::LogLambdaElectron() const
{
	// Convert to NRL Formulary Units
	double neNRL = ElectronDensity * 1e14; // We use 10^20 / m^3 = 10^14 / cm^3
	double TeNRL = ElectronTemperature * 1000; // We normalize to 1 keV they use 1 ev
	// For sensible values of the coulomb logarithm, the lambda_ee value in the NRL formulary
	// can be simplified to be the same as the middle lambda_ei value.
	return 24.0 - 0.5*::log( neNRL ) + ::log( TeNRL );
}

// tau_ee
double MirrorPlasma::ElectronCollisionTime() const
{
	double PiThreeHalves = ::pow( M_PI, 1.5 ); // pi^(3/2)
	double TeThreeHalves = ::pow( ElectronTemperature * ReferenceTemperature, 1.5 );
	double ZIon = pVacuumConfig->IonSpecies.Charge;
	return 12 * ::sqrt( ElectronMass ) * PiThreeHalves * TeThreeHalves * VacuumPermittivity * VacuumPermittivity / ( ::sqrt(2) * IonDensity * ReferenceDensity * ::pow( ZIon, 2 ) * ::pow( ElectronCharge, 4 ) * LogLambdaElectron() );
}

// NRL Formulary p34
double MirrorPlasma::LogLambdaIon() const
{
	double TiNRL = IonTemperature * 1000;
	double niNRL = IonDensity * 1e14;
	double ZIon = pVacuumConfig->IonSpecies.Charge;
	return 23.0 - 0.5 * ::log( niNRL ) - 1.5 * ::log( ZIon * ZIon / TiNRL );
}

double MirrorPlasma::IonCollisionTime() const
{
	double PiThreeHalves = ::pow( M_PI, 1.5 ); // pi^(3/2)
	double TiThreeHalves = ::pow( IonTemperature * ReferenceTemperature, 1.5 );
	double ZIon = pVacuumConfig->IonSpecies.Charge;
	return 12 * ::sqrt( pVacuumConfig->IonSpecies.Mass * ProtonMass ) * PiThreeHalves * TiThreeHalves * VacuumPermittivity * VacuumPermittivity / ( ::sqrt(2) * IonDensity * ReferenceDensity * ::pow( ZIon * ElectronCharge, 4 ) * LogLambdaIon() );
}

// Leading order contribution to Phi_0 in O(M^2)
// in units of T_e/e
double MirrorPlasma::CentrifugalPotential() const
{
	double tau = IonTemperature / ElectronTemperature;
	return -( 0.5/tau ) * ( 1.0 - 1.0 / pVacuumConfig->MirrorRatio ) * MachNumber * MachNumber / ( pVacuumConfig->IonSpecies.Charge / tau + 1 );
}

// Chi_e Defined in units of T_e
double MirrorPlasma::ParallelElectronPastukhovLossRate( double Chi_e ) const
{
	// For consistency, the integral in Pastukhov's paper is 1.0, as the
	// entire theory is an expansion in M^2 >> 1
	double R = pVacuumConfig->MirrorRatio;
	double tau_ee = ElectronCollisionTime();
	double Sigma = 1.0 + Zeff; // Include collisions with ions and impurities as well as self-collisions
	double LossRate = ( M_2_SQRTPI / tau_ee ) * Sigma * ElectronDensity * ReferenceDensity * ( 1.0 / ::log( R * Sigma ) ) * ( ::exp( - Chi_e ) / Chi_e );

	// To prevent false solutions, apply strong losses if the Mach number drops
	if ( Chi_e < 1.0 ) {
		double BaseLossRate = ElectronDensity * ReferenceDensity * ( SoundSpeed() / pVacuumConfig->PlasmaLength );
		double smoothing = Transition( Chi_e, .5, 1.0 );
		return smoothing*BaseLossRate + ( 1-smoothing )*LossRate;
	}

	return LossRate*pVacuumConfig->ParallelFudgeFactor;
}

double MirrorPlasma::ParallelElectronParticleLoss() const
{
	if ( pVacuumConfig->Collisional ) {
		// Particle loss from the mirror throat
		// given by the density at the throat and the sounds transit time
		double MirrorThroatDensity = IonDensity * ReferenceDensity * ::exp( CentrifugalPotential() );
#ifdef DEBUG
		std::cout << "Electron parallel particle loss is " << SoundSpeed() * MirrorThroatDensity << "\t";
		std::cout << "Collisionless parallel losse would have been " << ParallelElectronPastukhovLossRate( -AmbipolarPhi() );
#endif
		return SoundSpeed() * MirrorThroatDensity;
	}
	double Chi_e = -AmbipolarPhi(); // Ignore small electron mass correction
	return ParallelElectronPastukhovLossRate( Chi_e );
}

double MirrorPlasma::ParallelElectronHeatLoss() const
{
	if ( pVacuumConfig->Collisional )
	{
		double kappa_parallel = 3.16 * ElectronDensity * ElectronTemperature * ReferenceDensity * ReferenceTemperature * ElectronCollisionTime() / ( ElectronMass  );
		double L_parallel = pVacuumConfig->PlasmaLength;
#ifdef DEBUG
		std::cout << "Electron parallel heat flux is " << kappa_parallel * ElectronTemperature * ReferenceTemperature / ( L_parallel * L_parallel ) << std::endl;
		std::cout << "Collisionless parallel heat flux would have been "
		          << ParallelElectronPastukhovLossRate( -AmbipolarPhi() ) * ( ElectronTemperature * ReferenceTemperature ) * (  1.0 - AmbipolarPhi() );
#endif
		return kappa_parallel * ElectronTemperature * ReferenceTemperature / ( L_parallel * L_parallel );
	}

	// Energy loss per particle is ~ e Phi + T_e
	// AmbipolarPhi = e Phi / T_e so loss is T_e * ( AmbipolarPhi + 1)
	double Chi_e = -AmbipolarPhi(); // Ignore small electron mass correction
	// Particle energy is roughly T_e + Chi_e (thermal + potential)
	return ParallelElectronPastukhovLossRate( Chi_e ) * ( ElectronTemperature * ReferenceTemperature ) * ( Chi_e + 1.0 );
}

// Chi_i Defined in units of T_i
double MirrorPlasma::ParallelIonPastukhovLossRate( double Chi_i ) const
{
	// For consistency, the integral in Pastukhov's paper is 1.0, as the
	// entire theory is an expansion in M^2 >> 1
	double R = pVacuumConfig->MirrorRatio;
	double tau_ii = IonCollisionTime();
	double Sigma = 1.0;
	double LossRate = ( M_2_SQRTPI / tau_ii ) * Sigma * IonDensity * ReferenceDensity * ( 1.0 / ::log( R * Sigma ) ) * ( ::exp( - Chi_i ) / Chi_i );

	// To prevent false solutions, apply strong losses if the Mach number drops
	if ( Chi_i < 1.0 ) {
		double BaseLossRate = IonDensity * ReferenceDensity * ( SoundSpeed() / pVacuumConfig->PlasmaLength );
		double smoothing = Transition( Chi_i, .5, 1.0 );
		return smoothing*BaseLossRate + ( 1-smoothing )*LossRate;
	}

	return LossRate*pVacuumConfig->ParallelFudgeFactor;
}

double MirrorPlasma::Chi_i( double Phi ) const
{
	return pVacuumConfig->IonSpecies.Charge * Phi * ( ElectronTemperature/IonTemperature ) + 0.5 * MachNumber * MachNumber * ( 1.0 - 1.0/pVacuumConfig->MirrorRatio ) * ( ElectronTemperature / IonTemperature );
}

double MirrorPlasma::Chi_i() const
{
	return Chi_i( AmbipolarPhi() );
}

double MirrorPlasma::ParallelIonParticleLoss() const
{
	if ( pVacuumConfig->Collisional ) {
		// Particle loss at the sound speed from the mirror throat
		double MirrorThroatDensity = IonDensity * ReferenceDensity * ::exp( CentrifugalPotential() );
		return SoundSpeed() * MirrorThroatDensity;
	}

	// Electrostatic energy + centrifugal potential energy
	return ParallelIonPastukhovLossRate( Chi_i() );
}

double MirrorPlasma::ParallelIonHeatLoss() const
{
	if ( pVacuumConfig->Collisional ) {
		// Collisional parallel heat transport
		double IonMass = pVacuumConfig->IonSpecies.Mass * ProtonMass;
		double kappa_parallel = 3.9 * IonDensity * IonTemperature * ReferenceDensity * ReferenceTemperature * IonCollisionTime() / ( IonMass );
		double L_parallel = pVacuumConfig->PlasmaLength;
		return kappa_parallel * IonTemperature * ReferenceTemperature / ( L_parallel * L_parallel );
	}

	// Energy loss per particle is ~ Chi_i + T_i
	return ParallelIonPastukhovLossRate( Chi_i() ) * ( IonTemperature * ReferenceTemperature ) * ( ::fabs( Chi_i() )  + 1.0 );
}
/*
double MirrorPlasma::ParallelCurrent( double Phi ) const
{
	double Chi_i = pVacuumConfig->IonSpecies.Charge * Phi * ( ElectronTemperature/IonTemperature ) +
		0.5 * MachNumber * MachNumber * ( 1.0 - 1.0/pVacuumConfig->MirrorRatio ) * ( ElectronTemperature / IonTemperature );
	double Chi_e = -Phi; // Ignore small electron mass correction

	// If Alphas are included, they correspond to a (small) charge flow
	if ( pVacuumConfig->AlphaHeating )
	{
		double AlphaLossRate =  AlphaProductionRate() * PromptAlphaLossFraction();
		return 2.0*AlphaLossRate + ParallelIonPastukhovLossRate( Chi_i )*pVacuumConfig->IonSpecies.Charge - ParallelElectronPastukhovLossRate( Chi_e );
	}
	else
	{
		return ParallelIonPastukhovLossRate( Chi_i )*pVacuumConfig->IonSpecies.Charge - ParallelElectronPastukhovLossRate( Chi_e );
	}
}
*/

// Sets Phi to the ambipolar Phi required such that ion loss = electron loss
double MirrorPlasma::AmbipolarPhi() const
{
	double AmbipolarPhi = CentrifugalPotential();

	if ( pVacuumConfig->Collisional )
		return AmbipolarPhi;

	if ( pVacuumConfig->AmbipolarPhi ) {
		// Add correction.
		double Sigma = 1.0 + Zeff;
		double R = pVacuumConfig->MirrorRatio;
		double Correction = ::log( (  ElectronCollisionTime() / IonCollisionTime() ) * ( ::log( R*Sigma ) / ( Sigma * ::log( R ) ) ) );

		// This gives us a first-order guess for the Ambipolar potential. Now we solve j_|| = 0 to get the better answer.
		//
		auto ParallelCurrent = [ & ]( double Phi ) {
			double Chi_e = -Phi; // Ignore small electron mass correction

			// If Alphas are included, they correspond to a (small) charge flow
			if ( pVacuumConfig->AlphaHeating )
			{
				double AlphaLossRate =  AlphaProductionRate() * PromptAlphaLossFraction();
				return 2.0*AlphaLossRate + ParallelIonPastukhovLossRate( Chi_i( Phi ) )*pVacuumConfig->IonSpecies.Charge - ParallelElectronPastukhovLossRate( Chi_e );
			}
			else
			{
				return ParallelIonPastukhovLossRate( Chi_i( Phi ) )*pVacuumConfig->IonSpecies.Charge - ParallelElectronPastukhovLossRate( Chi_e );
			}
		};

		boost::uintmax_t iters = 1000;
		boost::math::tools::eps_tolerance<double> tol( 11 ); // only bother getting part in 1024 accuracy
		auto [ Phi_l, Phi_u ] = boost::math::tools::bracket_and_solve_root( ParallelCurrent, AmbipolarPhi, 1.2, false, tol, iters );
		AmbipolarPhi = ( Phi_l + Phi_u )/2.0;

		if ( ::fabs( Phi_u - Phi_l )/2.0 > ::fabs( 0.01*AmbipolarPhi ) )
		{
			std::cerr << "Unable to find root of j_|| = 0, using approximation" << std::endl;
			return CentrifugalPotential() + Correction/2.0;
		}
	}

	return AmbipolarPhi;
}

double MirrorPlasma::ParallelKineticEnergyLoss() const
{
	double IonLossRate = ParallelIonParticleLoss();
	double EndExpansionRatio = 1.0; // If particles are hitting the wall at a radius larger than they are confined at, this increases momentum loss per particle
	// M^2 = u^2 / c_s^2 = m_i u^2 / T_e
	double KineticEnergyPerIon = 0.5 * ElectronTemperature * ReferenceTemperature * ( EndExpansionRatio * EndExpansionRatio ) *  MachNumber * MachNumber;
	// Convert to W/m^3 for consistency across loss rates
	return IonLossRate * KineticEnergyPerIon;
}

double MirrorPlasma::ClassicalIonHeatLoss() const
{
	double omega_ci = IonCyclotronFrequency();
	double IonMass = pVacuumConfig->IonSpecies.Mass * ProtonMass;
	double kappa_perp = 2.0 * IonDensity * IonTemperature * ReferenceTemperature * ReferenceDensity / ( IonMass * omega_ci * omega_ci * IonCollisionTime() );
	double L_T = ( pVacuumConfig->PlasmaColumnWidth / 2.0 );
	// Power density in W/m^3
	return kappa_perp * IonTemperature * ReferenceTemperature /( L_T * L_T );
}

double MirrorPlasma::ClassicalElectronHeatLoss() const
{
	double omega_ce = ElectronCyclotronFrequency();
	double kappa_perp = 4.66 * ElectronDensity * ElectronTemperature * ReferenceTemperature * ReferenceDensity / ( ElectronMass * omega_ce * omega_ce * ElectronCollisionTime() );
	double L_T = ( pVacuumConfig->PlasmaColumnWidth / 2.0 );
	// Power density in W/m^3
	return kappa_perp * ElectronTemperature * ReferenceTemperature /( L_T * L_T );
}

double MirrorPlasma::ClassicalHeatLosses() const
{
	return ClassicalIonHeatLoss() + ClassicalElectronHeatLoss();
}

double MirrorPlasma::RadiationLosses() const
{
	return BremsstrahlungLosses() + CyclotronLosses();
}

// Formula from the NRL formulary page 58 and the definition of Z_eff:
// n_e Z_eff = Sum_i n_i Z_i^2 (sum over all species that aren't electrons)
double MirrorPlasma::BremsstrahlungLosses() const
{
	// NRL formulary with reference values factored out
	// Return units are W/m^3
	return 169 * ::sqrt( 1000 * ElectronTemperature ) * Zeff * ElectronDensity * ElectronDensity;
}

// Formula (34) on page 58 of the NRL formulary is the vacuum emission
// we use the modified loss rate given in Tamor (1988) including a transparency factor
double MirrorPlasma::CyclotronLosses() const
{
	// NRL formulary with reference values factored out
	// Return units are W/m^3
	double B_central = pVacuumConfig->CentralCellFieldStrength; // in Tesla
	double P_vacuum = 6.21 * 1000 * ElectronDensity * ElectronTemperature * B_central * B_central;

	// Characteristic absorption length
	// lambda_0 = (Electron Inertial Lenght) / ( Plasma Frequency / Cyclotron Frequency )  ; Eq (4) of Tamor
	//				= (5.31 * 10^-4 / (n_e20)^1/2) / ( 3.21 * (n_e20)^1/2 / B ) ; From NRL Formulary, converted to our units (Tesla for B & 10^20 /m^3 for n_e)
	double LambdaZero = ( 5.31e-4 / 3.21 ) * ( B_central / ElectronDensity );
	double WallReflectivity = 0.95;
	double OpticalThickness = ( pVacuumConfig->PlasmaColumnWidth / ( 1.0 - WallReflectivity ) ) / LambdaZero;
	// This is the Phi introduced by Trubnikov and later approximated by Tamor 
	double TransparencyFactor = ::pow( ElectronTemperature, 1.5 ) / ( 200.0 * ::sqrt( OpticalThickness ) );
	// Moderate the vacuum emission by the transparency factor
	return P_vacuum * TransparencyFactor;
}

double MirrorPlasma::Beta() const
{
	// From Plasma Formulary, so convert to /cm^3 , eV, and Gauss
	double ne_Formulary = ElectronDensity * ReferenceDensity * 1e-6;
	double Te_Formulary = ElectronTemperature * 1e3;
	double ni_Formulary = IonDensity * ReferenceDensity * 1e-6;
	double Ti_Formulary = IonTemperature * 1e3;
	double MagField_Formulary = pVacuumConfig->CentralCellFieldStrength * 1e4;
	return 4.03e-11 * ( ne_Formulary * Te_Formulary + ni_Formulary * Ti_Formulary ) / ( MagField_Formulary * MagField_Formulary );
}

double MirrorPlasma::DebyeLength() const
{
	// l_debye^2 = epsilon_0 * (k_B T_e) / n_e * e^2
	double LambdaDebyeSquared = VacuumPermittivity * ( ElectronTemperature * ReferenceTemperature ) / ( ElectronDensity * ReferenceDensity * ElectronCharge * ElectronCharge );
	return ::sqrt( LambdaDebyeSquared );
}


double MirrorPlasma::NuStar() const
{
	return pVacuumConfig->PlasmaLength / ( IonCollisionTime() * SoundSpeed() );
}


double MirrorPlasma::KineticEnergy() const
{
	double IonMass = ProtonMass * pVacuumConfig->IonSpecies.Mass;
	double u = MachNumber * SoundSpeed();
	return .5 * IonMass * IonDensity * ReferenceDensity * u * u * pVacuumConfig->PlasmaVolume();
}

double MirrorPlasma::ThermalEnergy() const
{
	return 1.5 * ( ElectronDensity * ElectronTemperature + IonDensity * IonTemperature ) * ReferenceDensity * ReferenceTemperature * pVacuumConfig->PlasmaVolume();
}

// Braginskii eta_1
double MirrorPlasma::ClassicalViscosity() const
{
	double omega_ci = IonCyclotronFrequency();
	return pVacuumConfig->PerpFudgeFactor * ( 3.0 / 10.0 ) * ( IonDensity * ReferenceDensity * IonTemperature * ReferenceTemperature ) / ( omega_ci * omega_ci * IonCollisionTime() );
}


// Viscous heating = eta * u^2 / L_u^2
double MirrorPlasma::ViscousHeating() const
{
	double L_u = ( pVacuumConfig->PlasmaColumnWidth / 2.0 );
	double Velocity = MachNumber * SoundSpeed();
	double VelocityShear = Velocity / L_u;

	return ClassicalViscosity() * VelocityShear * VelocityShear;
}

double MirrorPlasma::ViscousTorque() const
{
	double L_u = ( pVacuumConfig->PlasmaColumnWidth / 2.0 );
	double Velocity = MachNumber * SoundSpeed();

	return ClassicalViscosity() * ( Velocity * pVacuumConfig->PlasmaCentralRadius() / ( L_u * L_u ) );
}

double MirrorPlasma::ClassicalElectronParticleLosses() const
{
	double omega_ce = ElectronCyclotronFrequency();
	double L_n = ( pVacuumConfig->PlasmaColumnWidth / 2.0 );
	double D = ElectronTemperature * ReferenceTemperature / ( ElectronMass * omega_ce * omega_ce * ElectronCollisionTime() );
	return ( D / ( L_n * L_n ) ) * ElectronDensity * ReferenceDensity;
}

double MirrorPlasma::ClassicalIonParticleLosses() const
{
	// just the pure plasma Ambipolar result
	return ClassicalElectronParticleLosses();
}

double MirrorPlasma::AlfvenMachNumber() const
{
	// v_A^2 = B^2 / mu_0 * n_i * m_i
	// c_s^2 = T_e / m_i
	double MassDensity = IonDensity * ReferenceDensity * pVacuumConfig->IonSpecies.Mass * ProtonMass;
	double AlfvenSpeed = pVacuumConfig->CentralCellFieldStrength / ::sqrt( PermeabilityOfFreeSpace * MassDensity );

	return MachNumber * ( SoundSpeed() / AlfvenSpeed );
}

double MirrorPlasma::CollisionalTemperatureEquilibrationTime() const
{
	return ElectronCollisionTime()/( (3./pVacuumConfig->IonSpecies.Mass)*(ElectronMass/ProtonMass) );
}

double MirrorPlasma::IonToElectronHeatTransfer() const
{
	double EnergyDensity = ( ElectronDensity * ReferenceDensity ) * ( IonTemperature - ElectronTemperature ) * ReferenceTemperature;
	// return EnergyDensity / ( 1e-6 ); // Start by ensuring equal temperatures ( force tau to be 1 µs )
	return EnergyDensity / CollisionalTemperatureEquilibrationTime();
}

double MirrorPlasma::CXHeatLosses() const
{
	double EnergyPerIon = IonTemperature * ReferenceTemperature;
	return CXLossRate() * EnergyPerIon;
}

double MirrorPlasma::IonHeatLosses() const
{
	return ClassicalIonHeatLoss() + ParallelIonHeatLoss() + CXHeatLosses();
}

double MirrorPlasma::ElectronHeatLosses() const
{
	return ParallelElectronHeatLoss() + RadiationLosses();
}

double MirrorPlasma::IonHeating() const
{
	double Heating = ViscousHeating();

	return Heating - IonToElectronHeatTransfer();
}

double MirrorPlasma::ElectronHeating() const
{
	double Heating = pVacuumConfig->AuxiliaryHeating * 1e6 / pVacuumConfig->PlasmaVolume(); // Auxiliary Heating stored as MW, heating is in W/m^3
	if ( pVacuumConfig->AlphaHeating ) {
		// 1e6 as we are using W/m^3 and the formulary was in MW/m^3
		Heating += AlphaHeating() * 1e6;
	}
	// std::cout << "e-Heating comprises " << Heating << " of aux and " << IonToElectronHeatTransfer() << " transfer" << std::endl;
	return Heating + IonToElectronHeatTransfer();
}


void MirrorPlasma::SetMachFromVoltage()
{
	// u = E x B / B^2
	// M = u/c_s ~ (V/aB)/cs
	MachNumber = pVacuumConfig->ImposedVoltage / ( pVacuumConfig->PlasmaColumnWidth * pVacuumConfig->CentralCellFieldStrength * SoundSpeed() );
}

double MirrorPlasma::AngularMomentumPerParticle() const
{
	return pVacuumConfig->IonSpecies.Mass * ProtonMass * SoundSpeed() * MachNumber * pVacuumConfig->PlasmaCentralRadius();
}

double MirrorPlasma::ParallelAngularMomentumLossRate() const
{
	double IonLoss = ParallelIonParticleLoss();
	return IonLoss * AngularMomentumPerParticle(); 
}

double MirrorPlasma::CXMomentumLosses() const
{
	double IonLoss = CXLossRate();
	return IonLoss * AngularMomentumPerParticle();
}

// Momentum Equation is
//		I d omega / dt = <Viscous Torque> + <Parallel Angular Momentum Loss> + R J_R B_z
//
// Div(J) = 0 => R J_R = constant
//
// Current I_radial = 2 * pi * R * J_R * L_plasma
//
double MirrorPlasma::RadialCurrent() const
{
	double Torque = -ViscousTorque();
	double ParallelLosses = -ParallelAngularMomentumLossRate();
	double CXLosses = -CXMomentumLosses();
	// Inertial term = m_i n_i R^2 d omega / dt ~= m_i n_i R^2 d  / dt ( E/ ( R*B) )
	//					~= m_i n_i (R/B) * d/dt ( V / a )
	double Inertia;
	if ( isTimeDependent )
		Inertia = pVacuumConfig->IonSpecies.Mass * ProtonMass * IonDensity * ( pVacuumConfig->PlasmaCentralRadius() / pVacuumConfig->CentralCellFieldStrength )
		            * VoltageFunction->prime( time );
	else
		Inertia = 0.0;

	// R J_R = (<Torque> + <ParallelLosses> + <Inertia>)/B_z
	// I_R = 2*Pi*R*L*J_R
	double I_radial = 2.0 * M_PI * pVacuumConfig->PlasmaLength * ( Torque + ParallelLosses + Inertia + CXLosses ) / pVacuumConfig->CentralCellFieldStrength;
	return I_radial;
}

// Thrust from ions leaving
// assume parallel kinetic energy contains Chi_i
double MirrorPlasma::ParallelIonThrust() const
{
	double ParallelKineticEnergy = ( Chi_i() + 1.0 ) * IonTemperature * ReferenceTemperature;
	double ParallelMomentum = ::sqrt( 2.0  *  ParallelKineticEnergy * pVacuumConfig->IonSpecies.Mass * ProtonMass );
	// Only half the particle loss, as the Thrust diagnostics need each end separately.
	return ParallelMomentum * ( ParallelIonParticleLoss() / 2.0 ) * pVacuumConfig->PlasmaVolume();
}


void MirrorPlasma::UpdateVoltage()
{
	if ( !isTimeDependent )
		return;
	else
		pVacuumConfig->ImposedVoltage = ( *VoltageFunction )( time );
}

void MirrorPlasma::SetTime( double new_time )
{
	time = new_time;
	UpdateVoltage();
}
