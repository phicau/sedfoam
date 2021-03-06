{
    surfaceScalarField alphaf = fvc::interpolate(alpha);
    surfaceScalarField betaf = scalar(1.0) - alphaf;

    volScalarField rUaA = 1.0/UaEqn.A();
    volScalarField rUbA = 1.0/UbEqn.A();

    rUaAf = fvc::interpolate(rUaA);
    surfaceScalarField rUbAf = fvc::interpolate(rUbA);

    volVectorField HabyA = Ua;
    HabyA = rUaA*UaEqn.H();
    volVectorField HbbyA = Ub;
    HbbyA = rUbA*UbEqn.H();
    word schemeA("div(alpha,nu)");
 
    // limiter for the explicit stress terms to avoid numerical instability and/or
    // too strong limitation on the time step
    volScalarField nuFraExp = nuFra;
    nuFraExp.min(nuMaxExp);
    nuFraExp.correctBoundaryConditions();

    // interpolate 1/(alpha+alphaSmall) on the faces
    surfaceScalarField oneOverAlpha =
    (
        fvc::interpolate
        (
            scalar(1.0)/(alpha+alphaSmall),
            "interpolate(oneoveralpha)"
        )
    );

    // local Schmidt number
    if (SUSlocal)
    {
      volScalarField ustar = sqrt(mag(beta*nuEffb*(fvc::grad(Ub)+gradUbT)));
      SUStilde = scalar(1.0) + scalar(2.0)*pow(Ufall/(ustar+Usmall),2);
      SUStilde.min(scalar(3.0));
      SUS = SUStilde;
    
      Info<<"ustarmax"<<max(ustar).value()<<endl;
    }
    // Momentum fluxes computed on the faces for the particle phase phiDraga and
    // the fluid phase phiDragb
    surfaceScalarField phiDraga =
    (
    // explicit drag term
        //bonamy fvc::interpolate(beta/rhoa*K*rUaA)*phib 
    // turbulent suspension term (comment the next 2 lines if suspension term in UaEqn: susa)
    - (fvc::interpolate(SUS*(K/rhoa)*turbulenceb->nut()*(beta/(alpha + alphaSmall)))
       *fvc::snGrad(alpha))*rUaAf*mesh.magSf() 
    // grad(pa)
      - (oneOverAlpha/rhoa*fvc::snGrad(pa))*rUaAf*mesh.magSf()
    // grad(Pf)
    // - (oneOverAlpha/rhoa*fvc::snGrad(pff))*rUaAf*mesh.magSf() 
    // diagonal part of stress
      + (oneOverAlpha/rhoa*fvc::snGrad(lambdaUa*tr(gradUaT)))*rUaAf*mesh.magSf()
    // diagonal part of stress 2 normalement  sqr(Ct) à la place de 0
      - (
            scalar(2.0/3.0)*oneOverAlpha
           *fvc::snGrad((alpha+alphaSmall)*(0*turbulenceb->k()
          + nuEffa*tr(gradUaT)))
        )
       *rUaAf*mesh.magSf() 
      - (scalar(2.0/3.0)*oneOverAlpha*fvc::snGrad(nuFraExp*tr(gradUaT)))
       *rUaAf*mesh.magSf() 
    // stress part 3
      + rUaAf*(oneOverAlpha
       *fvc::interpolate(fvc::div((alpha+alphaSmall)*nuEffa*gradUaT, schemeA))
      & mesh.Sf())
      + rUaAf*(oneOverAlpha
       *fvc::interpolate(fvc::div(nuFraExp*gradUaT, schemeA)) & mesh.Sf())
    // gravity
      + rUaAf
       *(
           (1.0-rhob/rhoa)*(g & mesh.Sf())
        )
//      + rUaAf*(g & mesh.Sf())
    // mean pressure gradient
      + rUaAf*((gradPOSC/(tilt*rhob +(1.0-tilt)*rhoa)) & mesh.Sf())
    );

    surfaceScalarField phiDragb =
    (
// explicit drag term
//bonamy        fvc::interpolate(alpha/rhob*K*rUbA)*phia 
// turbulent suspension term (comment the next 2 lines if suspension term in UbEqn: susb)
        (fvc::interpolate(SUS*(K/rhob)*turbulenceb->nut())*fvc::snGrad(alpha))
        *rUbAf*mesh.magSf() 
// diagnoal part of stress
      - (fvc::interpolate(scalar(2.0)/(beta*scalar(3.0)))
       *fvc::snGrad(((nuEffb*tr(gradUbT)))*beta))*rUbAf*mesh.magSf() 
// stress part 3 
      + rUbAf*(fvc::interpolate(scalar(1.0)/beta)
       *fvc::interpolate(fvc::div(beta*nuEffb*gradUbT,schemeA)) & mesh.Sf())
// Gravity force
//      - rUbAf
//           (g & mesh.Sf())
//       +rUbAf*(g & mesh.Sf())
// driving force
      + rUbAf*((gradPOSC/rhob) & mesh.Sf())
    );

    //    fvOptions.makeRelative(phiHabyA);
    //    fvOptions.makeRelative(phiHbbyA);


    // Fix for gravity on outlet boundary.
    forAll(p_rbgh.boundaryField(), patchi)
    {
        if (isA<zeroGradientFvPatchScalarField>(p_rbgh.boundaryField()[patchi]))
        {
            phiDraga.boundaryFieldRef()[patchi] = 0.0;
            phiDragb.boundaryFieldRef()[patchi] = 0.0;
        }
        if (isA<symmetryFvPatchScalarField>(p_rbgh.boundaryField()[patchi]))
        {
            phiDraga.boundaryFieldRef()[patchi] = 0.0;
            phiDragb.boundaryFieldRef()[patchi] = 0.0;
        }
        if (p_rbgh.boundaryField().types()[patchi] == "symmetryPlane")
        {
            phiDraga.boundaryFieldRef()[patchi] = 0.0;
            phiDragb.boundaryFieldRef()[patchi] = 0.0;
        }
        if (p_rbgh.boundaryField().types()[patchi] == "groovyBC")
        {
            phiDraga.boundaryFieldRef()[patchi] = 0.0;
            phiDragb.boundaryFieldRef()[patchi] = 0.0;
        }
	if (p_rbgh.boundaryField().types()[patchi] == "outletInlet")
        {
            phiDraga.boundaryFieldRef()[patchi] = 0.0;
            phiDragb.boundaryFieldRef()[patchi] = 0.0;
        }
    }

    surfaceScalarField phiUa =
    (
        (fvc::interpolate(HabyA) & mesh.Sf()) + rUaAf*fvc::ddtCorr(Ua, phia)
    );

    surfaceScalarField phiUb =
    (
        (fvc::interpolate(HbbyA) & mesh.Sf()) + rUbAf*fvc::ddtCorr(Ub, phib)
    );

    surfaceScalarField phiaHbyA = phiUa + phiDraga;
    surfaceScalarField phibHbyA = phiUb + phiDragb;

    // Face-drag coefficients 
    surfaceScalarField rAUKda(fvc::interpolate(beta/rhoa*K*rUaA));
    surfaceScalarField rAUKdb(fvc::interpolate(alpha/rhob*K*rUbA));

    surfaceScalarField phiHbyA = alphaf*(phiaHbyA + rAUKda*phib) + betaf*(phibHbyA + rAUKdb*phia);

//    surfaceScalarField phiHbyA = alphaf*phiaHbyA + betaf*phibHbyA;

//    adjustPhi(phiHbyA, U, p_rbgh);

    // Poisson equation for the pressure 
    surfaceScalarField Dp
    (
        "(rho*(1|A(U)))",
        alphaf*rUaAf/rhoa + betaf*rUbAf/rhob
    );

    // Update the fixedFluxPressure BCs to ensure flux consistency
    setSnGrad<fixedFluxPressureFvPatchScalarField>
    (
        p_rbgh.boundaryFieldRef(),
        (
            phiHbyA.boundaryField()
          - (alphaf.boundaryField()*(mesh.Sf().boundaryField() & Ua.boundaryField())
          + betaf.boundaryField()*(mesh.Sf().boundaryField() & Ub.boundaryField())
            )
         )
        /(mesh.magSf().boundaryField()*Dp.boundaryField())
    );

    while (pimple.correctNonOrthogonal())
    {
        fvScalarMatrix pEqn
        (
            fvm::laplacian(Dp, p_rbgh) == fvc::div(phiHbyA)
        );

        pEqn.setReference(pRefCell, getRefCellValue(p_rbgh, pRefCell));
        pEqn.solve(mesh.solver(p_rbgh.select(pimple.finalInnerIter())));

        if (pimple.finalNonOrthogonalIter())
        {
            surfaceScalarField SfGradp = pEqn.flux()/Dp;

            surfaceScalarField phias = phiaHbyA - rUaAf*SfGradp/rhoa;
            surfaceScalarField phibs = phibHbyA - rUbAf*SfGradp/rhob;
//            phi = alphaf*phia + betaf*phib;
            phi = phiHbyA - pEqn.flux();

            surfaceScalarField phir
            (
                ((phias + rAUKda*phibs) - (phibs + rAUKdb*phias))
               /(1 - rAUKda*rAUKdb)
            );

            phia = phi + betaf*(phir);
            phib = phi - alphaf*(phir);

            #include "continuityErrs.H"
            p_rbgh.relax();
            SfGradp = pEqn.flux()/Dp;
            // Velocity correction step    
            volVectorField Ubs = HbbyA + rUbA*fvc::reconstruct(phiDragb/rUbAf-SfGradp/rhob);
            volVectorField Uas = HabyA + rUaA*fvc::reconstruct(phiDraga/rUaAf-SfGradp/rhoa);

            volScalarField Da(beta/rhoa*K*rUaA);
            volScalarField Db(alpha/rhob*K*rUbA);
            U = alpha*(Uas + Da*Ub) + beta*(Ubs + Db*Ua);
            volVectorField Ur(((1 - Db)*Uas - (1 - Da)*Ubs)/(1 - Da*Db));
            
            Ua = U + beta*Ur;
            Ua.correctBoundaryConditions();
            //fvOptions.correct(Ua);
            
            Ub = U - alpha*Ur;
            Ub.correctBoundaryConditions();
            //fvOptions.correct(Ub);
            
/*            phia = phiaHbyA - rUaAf*SfGradp/rhoa;
            phib = phibHbyA - rUbAf*SfGradp/rhob;
            phi = phiHbyA - pEqn.flux();

            #include "continuityErrs.H"
*/
            p = p_rbgh + rhob*gh;
            if (p_rbgh.needReference())
            {
                p += dimensionedScalar
                (
                    "p",
                    p.dimensions(),
                    pRefValue - getRefCellValue(p, pRefCell)
                );
                p_rbgh = p - rhob*gh;
            }
/*
            // Velocity correction step    
            Ub = HbbyA + rUbA*fvc::reconstruct(phiDragb/rUbAf-SfGradp/rhob);
            Ub.correctBoundaryConditions();
            Ua = HabyA + rUaA*fvc::reconstruct(phiDraga/rUaAf-SfGradp/rhoa);
*/
            #include "UaUbLimiter.H"
            Ua.correctBoundaryConditions();
            // Update mixture velocity    
            U = alpha*Ua + beta*Ub;
            U.correctBoundaryConditions();
        }
    }
}
