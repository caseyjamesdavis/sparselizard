#include "impliciteuler.h"

impliciteuler::impliciteuler(formulation formul, vec initsolution, std::vector<bool> isrhskcconstant)
{
    myformulation = formul;
    
    x = initsolution;
    
    if (isrhskcconstant.size() == 0)
        isconstant = {false,false,false};
    else
        isconstant = isrhskcconstant;
    if (isconstant.size() != 3)
    {
        std::cout << "Error in 'impliciteuler' object: expected a length 3 or empty vector as third argument" << std::endl;
        abort();  
    }
}

std::vector<vec> impliciteuler::runlinear(double starttime, double timestep, double endtime, int outputeverynthtimestep, int verbosity)
{
    return run(true, starttime, timestep, endtime, outputeverynthtimestep, verbosity);
}

std::vector<vec> impliciteuler::runnonlinear(double starttime, double timestep, double endtime, int outputeverynthtimestep, int verbosity)
{
    return run(false, starttime, timestep, endtime, outputeverynthtimestep, verbosity);
}

std::vector<vec> impliciteuler::run(bool islinear, double starttime, double timestep, double endtime, int outputeverynthtimestep, int verbosity)
{
    if (starttime > endtime)
        return {};
    
    if (outputeverynthtimestep <= 0)
        outputeverynthtimestep = 1;
        
    // Get all fields in the formulation:
    shared_ptr<dofmanager> dofmngr = myformulation.getdofmanager();
    std::vector<shared_ptr<rawfield>> allfields = dofmngr->getfields();
    // Set all fields in the formulation to the initial solution:
    for (int i = 0; i < allfields.size(); i++)
        allfields[i]->setdata(-1, x|field(allfields[i]));
    
    // Remove leftovers (if any):
    myformulation.rhs(); myformulation.K(); myformulation.C();
    
    vec rhs; mat K, C, leftmat;
    
    // Count the number of time steps to step through and the number of vectors to output:
    int numtimesteps = 0; int outputsize = 0;
    for (double t = starttime; t <= endtime; t = t + timestep)
    {
        if (numtimesteps%outputeverynthtimestep == 0)
            outputsize++;
        numtimesteps++;
    }
    
    
    // Start the implicit Euler iteration:
    std::cout << "Implicit Euler for " << numtimesteps << " timesteps in range " << starttime << " to " << endtime << " sec:" << std::endl;
    std::vector<vec> output(outputsize);
    output[0] = x;
    
    // We already have everything for time step 0 so we start at 1:
    int timestepindex = 1;
    for (double t = starttime + timestep; t <= endtime; t = t + timestep)
    {        
        std::cout << timestepindex << "@" << t << "sec" << std::flush;

        mathop::settime(t);
        
        // Nonlinear loop:
        double relchange = 1; int nlit = 0;
        vec xnext = x;
        while (relchange > tol)
        {
            vec xtolcalc = xnext;
            
            // Reassemble only the non-constant matrices:
            if (isconstant[1] == false || timestepindex == 1)
            {
                myformulation.generatestiffnessmatrix();
                K = myformulation.K(false, false);
            }
            if (isconstant[2] == false || timestepindex == 1)
            {
                myformulation.generatedampingmatrix();
                C = myformulation.C(false, true);
            }
            if (isconstant[0] == false || timestepindex == 1)
            {
                myformulation.generaterhs();
                rhs = myformulation.rhs();
            }
            else
                rhs.updateconstraints();
            
            // Reuse matrices when possible (including the LU decomposition):
            if (isconstant[1] == false || isconstant[2] == false || timestepindex == 1)
            {
                leftmat = C + timestep*K;
                leftmat.reuselu();
            }
            
            // Update the solution xnext.
            xnext = mathop::solve(leftmat, C*xnext+timestep*rhs);
            
            // Update all fields in the formulation:
            for (int i = 0; i < allfields.size(); i++)
                allfields[i]->setdata(-1, xnext|field(allfields[i]));
            
            relchange = (xnext-xtolcalc).norm()/xnext.norm();
            
            if (islinear == false && verbosity > 0)
                std::cout << " " << relchange << std::flush;

            nlit++; 
            
            if (islinear)
                break;
        }

        x = xnext;
        
        if (islinear == false)
            std::cout << " (" << nlit << "NL it)" << std::flush;
        if (timestepindex < numtimesteps-1)
        std::cout << " -> " << std::flush;
        
        // Only one every 'outputeverynthtimestep' solutions is output:
        if (timestepindex%outputeverynthtimestep == 0)
            output[timestepindex/outputeverynthtimestep] = x;
        timestepindex++;
    }
    std::cout << std::endl;
    
    return output;
}

